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

#ifndef _MT_PMIC_API_H_
#define _MT_PMIC_API_H_

extern unsigned int mt6355_upmu_get_hwcid(
	void);
extern unsigned int mt6355_upmu_get_swcid(
	void);
extern unsigned int mt6355_upmu_set_rg_srclken_in0_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_srclken_in0_en(
	void);
extern unsigned int mt6355_upmu_set_rg_srclken_in1_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_srclken_in1_en(
	void);
extern unsigned int mt6355_upmu_set_rg_osc_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_srclken_in0_hw_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_srclken_in1_hw_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_osc_sel_hw_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_srclken_in_sync_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_srclken_in_sync_en(
	void);
extern unsigned int mt6355_upmu_set_rg_osc_en_auto_off(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_osc_en_auto_off(
	void);
extern unsigned int mt6355_upmu_get_test_out(
	void);
extern unsigned int mt6355_upmu_set_rg_mon_flag_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_mon_grp_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_nandtree_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_test_auxadc(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_efuse_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_test_strup(
	unsigned int val);
extern unsigned int mt6355_upmu_set_testmode_sw(
	unsigned int val);
extern unsigned int mt6355_upmu_get_va12_pg_deb(
	void);
extern unsigned int mt6355_upmu_get_va10_pg_deb(
	void);
extern unsigned int mt6355_upmu_get_vsram_gpu_pg_deb(
	void);
extern unsigned int mt6355_upmu_get_vsram_md_pg_deb(
	void);
extern unsigned int mt6355_upmu_get_vsram_core_pg_deb(
	void);
extern unsigned int mt6355_upmu_get_va18_pg_deb(
	void);
extern unsigned int mt6355_upmu_get_buck_rsv_pg_deb(
	void);
extern unsigned int mt6355_upmu_get_vdram2_pg_deb(
	void);
extern unsigned int mt6355_upmu_get_vdram1_pg_deb(
	void);
extern unsigned int mt6355_upmu_get_vproc12_pg_deb(
	void);
extern unsigned int mt6355_upmu_get_vproc11_pg_deb(
	void);
extern unsigned int mt6355_upmu_get_vs1_pg_deb(
	void);
extern unsigned int mt6355_upmu_get_vmodem_pg_deb(
	void);
extern unsigned int mt6355_upmu_get_vgpu_pg_deb(
	void);
extern unsigned int mt6355_upmu_get_vcore_pg_deb(
	void);
extern unsigned int mt6355_upmu_get_vs2_pg_deb(
	void);
extern unsigned int mt6355_upmu_get_ext_pmic_pg_deb(
	void);
extern unsigned int mt6355_upmu_get_vxo18_pg_deb(
	void);
extern unsigned int mt6355_upmu_get_vxo22_pg_deb(
	void);
extern unsigned int mt6355_upmu_get_vusb33_pg_deb(
	void);
extern unsigned int mt6355_upmu_get_vsram_proc_pg_deb(
	void);
extern unsigned int mt6355_upmu_get_vio28_pg_deb(
	void);
extern unsigned int mt6355_upmu_get_vufs18_pg_deb(
	void);
extern unsigned int mt6355_upmu_get_vemc_pg_deb(
	void);
extern unsigned int mt6355_upmu_get_vio18_pg_deb(
	void);
extern unsigned int mt6355_upmu_get_strup_va12_pg_status(
	void);
extern unsigned int mt6355_upmu_get_strup_va10_pg_status(
	void);
extern unsigned int mt6355_upmu_get_strup_vsram_gpu_pg_status(
	void);
extern unsigned int mt6355_upmu_get_strup_vsram_md_pg_status(
	void);
extern unsigned int mt6355_upmu_get_strup_vsram_core_pg_status(
	void);
extern unsigned int mt6355_upmu_get_strup_va18_pg_status(
	void);
extern unsigned int mt6355_upmu_get_strup_buck_rsv_pg_status(
	void);
extern unsigned int mt6355_upmu_get_strup_vdram2_pg_status(
	void);
extern unsigned int mt6355_upmu_get_strup_vdram1_pg_status(
	void);
extern unsigned int mt6355_upmu_get_strup_vproc12_pg_status(
	void);
extern unsigned int mt6355_upmu_get_strup_vproc11_pg_status(
	void);
extern unsigned int mt6355_upmu_get_strup_vs1_pg_status(
	void);
extern unsigned int mt6355_upmu_get_strup_vmodem_pg_status(
	void);
extern unsigned int mt6355_upmu_get_strup_vgpu_pg_status(
	void);
extern unsigned int mt6355_upmu_get_strup_vcore_pg_status(
	void);
extern unsigned int mt6355_upmu_get_strup_vs2_pg_status(
	void);
extern unsigned int mt6355_upmu_get_strup_ext_pmic_pg_status(
	void);
extern unsigned int mt6355_upmu_get_strup_vxo18_pg_status(
	void);
extern unsigned int mt6355_upmu_get_strup_vxo22_pg_status(
	void);
extern unsigned int mt6355_upmu_get_strup_vusb33_pg_status(
	void);
extern unsigned int mt6355_upmu_get_strup_vsram_proc_pg_status(
	void);
extern unsigned int mt6355_upmu_get_strup_vio28_pg_status(
	void);
extern unsigned int mt6355_upmu_get_strup_vufs18_pg_status(
	void);
extern unsigned int mt6355_upmu_get_strup_vemc_pg_status(
	void);
extern unsigned int mt6355_upmu_get_strup_vio18_pg_status(
	void);
extern unsigned int mt6355_upmu_get_strup_buck_rsv_oc_status(
	void);
extern unsigned int mt6355_upmu_get_strup_vdram2_oc_status(
	void);
extern unsigned int mt6355_upmu_get_strup_vdram1_oc_status(
	void);
extern unsigned int mt6355_upmu_get_strup_vproc12_oc_status(
	void);
extern unsigned int mt6355_upmu_get_strup_vproc11_oc_status(
	void);
extern unsigned int mt6355_upmu_get_strup_vs1_oc_status(
	void);
extern unsigned int mt6355_upmu_get_strup_vmodem_oc_status(
	void);
extern unsigned int mt6355_upmu_get_strup_vgpu_oc_status(
	void);
extern unsigned int mt6355_upmu_get_strup_vcore_oc_status(
	void);
extern unsigned int mt6355_upmu_get_strup_vs2_oc_status(
	void);
extern unsigned int mt6355_upmu_get_pmu_thermal_deb(
	void);
extern unsigned int mt6355_upmu_get_strup_thermal_status(
	void);
extern unsigned int mt6355_upmu_get_pmu_test_mode_scan(
	void);
extern unsigned int mt6355_upmu_get_pwrkey_deb(
	void);
extern unsigned int mt6355_upmu_get_homekey_deb(
	void);
extern unsigned int mt6355_upmu_get_rtc_xtal_det_done(
	void);
extern unsigned int mt6355_upmu_get_xosc32_enb_det(
	void);
extern unsigned int mt6355_upmu_set_rtc_xtal_det_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_pmu_tdsel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_spi_tdsel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_aud_tdsel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_e32cal_tdsel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_pmu_rdsel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_spi_rdsel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_aud_rdsel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_e32cal_rdsel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_smt_wdtrstb_in(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_smt_homekey(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_smt_srclken_in0(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_smt_srclken_in1(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_smt_rtc_32k1v8_0(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_smt_rtc_32k1v8_1(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_smt_scp_vreq_vao(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_smt_spi_clk(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_smt_spi_csn(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_smt_spi_mosi(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_smt_spi_miso(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_smt_aud_clk(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_smt_aud_dat_mosi(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_smt_aud_dat_miso(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_smt_vow_clk_miso(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_octl_srclken_in0(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_octl_srclken_in1(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_octl_rtc_32k1v8_0(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_octl_rtc_32k1v8_1(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_octl_spi_clk(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_octl_spi_csn(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_octl_spi_mosi(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_octl_spi_miso(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_octl_aud_clk(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_octl_aud_dat_mosi(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_octl_aud_dat_miso(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_octl_vow_clk_miso(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_octl_homekey(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_octl_scp_vreq_vao(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_spi_clk_filter_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_spi_clk_filter_en(
	void);
extern unsigned int mt6355_upmu_set_rg_spi_csn_filter_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_spi_csn_filter_en(
	void);
extern unsigned int mt6355_upmu_set_rg_spi_mosi_filter_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_spi_mosi_filter_en(
	void);
extern unsigned int mt6355_upmu_set_rg_spi_miso_filter_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_spi_miso_filter_en(
	void);
extern unsigned int mt6355_upmu_set_rg_aud_clk_filter_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_aud_clk_filter_en(
	void);
extern unsigned int mt6355_upmu_set_rg_aud_dat_mosi_filter_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_aud_dat_mosi_filter_en(
	void);
extern unsigned int mt6355_upmu_set_rg_aud_dat_miso_filter_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_aud_dat_miso_filter_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vow_clk_miso_filter_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vow_clk_miso_filter_en(
	void);
extern unsigned int mt6355_upmu_set_rg_wdtrstb_in_filter_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_wdtrstb_in_filter_en(
	void);
extern unsigned int mt6355_upmu_set_rg_homekey_filter_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_homekey_filter_en(
	void);
extern unsigned int mt6355_upmu_set_rg_srclken_in0_filter_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_srclken_in0_filter_en(
	void);
extern unsigned int mt6355_upmu_set_rg_srclken_in1_filter_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_srclken_in1_filter_en(
	void);
extern unsigned int mt6355_upmu_set_rg_rtc32k_1v8_0_filter_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_rtc32k_1v8_0_filter_en(
	void);
extern unsigned int mt6355_upmu_set_rg_rtc32k_1v8_1_filter_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_rtc32k_1v8_1_filter_en(
	void);
extern unsigned int mt6355_upmu_set_rg_scp_vreq_vao_filter_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_scp_vreq_vao_filter_en(
	void);
extern unsigned int mt6355_upmu_set_rg_spi_clk_rcsel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_spi_csn_rcsel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_spi_mosi_rcsel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_spi_miso_rcsel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_aud_clk_rcsel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_aud_dat_mosi_rcsel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_aud_dat_miso_rcsel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vow_clk_miso_rcsel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_wdtrstb_in_rcsel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_homekey_rcsel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_srclken_in0_rcsel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_srclken_in1_rcsel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_rtc32k_1v8_0_rcsel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_rtc32k_1v8_1_rcsel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_scp_vreq_vao_rcsel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_top_status(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_srclken_in2_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_srclken_in2_en(
	void);
extern unsigned int mt6355_upmu_set_rg_srclken_in3_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_srclken_in3_en(
	void);
extern unsigned int mt6355_upmu_get_vm_mode(
	void);
extern unsigned int mt6355_upmu_set_rg_g_smps_ck_pdn(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_g_smps_test_ck_pdn(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_intrp_ck_pdn(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_intrp_pre_oc_ck_pdn(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_g_bif_1m_ck_pdn(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_bif_x1_ck_pdn(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_bif_x4_ck_pdn(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_bif_x72_ck_pdn(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_auxadc_ao_1m_ck_pdn(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_auxadc_ck_pdn(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_auxadc_rng_ck_pdn(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_auxadc_1m_ck_pdn(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_drv_32k_ck_pdn(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_top_ckpdn_con0_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_9m_ck_pdn(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_1m_ck_pdn(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_18m_ck_pdn(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_32k_ck_pdn(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_9m_ck_pdn(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_1m_ck_pdn(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_rsv0_ck_pdn(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_ana_clk_pdn(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_trim_75k_ck_pdn(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_chdet_75k_ck_pdn(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_spi_ck_pdn(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_reg_ck_pdn(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_bgr_test_ck_pdn(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_fqmtr_32k_ck_pdn(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_fqmtr_ck_pdn(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_ana_ck_pdn(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_eosc_cali_test_ck_pdn(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_rtc_eosc32_ck_pdn(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_rtc_sec_32k_ck_pdn(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_rtc_mclk_pdn(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_rtc_32k_ck_pdn(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_rtc_26m_ck_pdn(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_fgadc_ft_ck_pdn(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_fgadc_dig_ck_pdn(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_fgadc_ana_ck_pdn(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_strup_32k_ck_pdn(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_strup_75k_ck_pdn(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_rtc32k_1v8_0_o_pdn(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_rtc32k_1v8_1_o_pdn(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_rtc_2sec_off_det_pdn(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_smps_ck_div_pdn(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_baton_75k_ck_pdn(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_efuse_ck_pdn(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_accdet_ck_pdn(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_aud_ck_pdn(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audif_ck_pdn(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vow32k_ck_pdn(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vow12m_ck_pdn(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_zcd13m_ck_pdn(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_rtc_sec_mclk_pdn(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_auxadc_32k_ck_pdn(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_eint_32k_ck_pdn(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_top_ckpdn_con3_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_rtcdet_ck_pdn(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_rtc_75k_ck_pdn(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_fqmtr_ck_cksel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_75k_32k_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_fgadc_ana_ck_cksel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_bgr_test_ck_cksel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_aud_ck_cksel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audif_ck_cksel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_top_cksel_con0_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_srcvolten_sw(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_osc_sel_sw(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vowen_sw(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_srcvolten_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_osc_sel_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vowen_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_top_cksel_con2_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_auxadc_ck_divsel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_9m_ck_divsel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_9m_ck_divsel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_bif_x4_ck_divsel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_reg_ck_divsel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_top_ckdivsel_con0_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_g_smps_ck_pdn_hwen(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_smps_pd_1m_ck_pdn_hwen(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_auxadc_ck_pdn_hwen(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_auxadc_rng_ck_pdn_hwen(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_bif_x4_ck_pdn_hwen(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_bif_x72_ck_pdn_hwen(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_rtc_26m_ck_pdn_hwen(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_reg_ck_pdn_hwen(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_rtc_mclk_pdn_hwen(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_rtc_sec_32k_ck_pdn_hwen(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_efuse_ck_pdn_hwen(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_rtc_sec_mclk_pdn_hwen(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_auxadc_32k_ck_pdn_hwen(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_eint_32k_ck_pdn_hwen(
	unsigned int val);
extern unsigned int mt6355_upmu_set_top_ckhwen_con0_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_9m_ck_pdn_hwen(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_1m_ck_pdn_hwen(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_18m_ck_pdn_hwen(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_1m_ck_pdn_hwen(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_9m_ck_pdn_hwen(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vmodem_1m_ck_pdn_hwen(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_auxadc_1m_ck_pdn_hwen(
	unsigned int val);
extern unsigned int mt6355_upmu_set_top_ckhwen_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc11_freq_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc12_freq_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcore_freq_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vgpu_freq_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vdram1_freq_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vdram2_freq_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vmodem_freq_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_top_buck_anack_freq_sel_con0_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vs1_freq_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vs2_freq_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vpa_phs_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_top_buck_anack_freq_sel_con1_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_pmu75k_ck_tst_dis(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_smps_ck_tst_dis(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_rtc32k_ck_tst_dis(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_fg_ck_tst_dis(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_rtc26m_ck_tst_dis(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_aud26m_ck_tst_dis(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vow12m_ck_tst_dis(
	unsigned int val);
extern unsigned int mt6355_upmu_set_top_cktst_con0_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_ana_auto_off_dis(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_auxadc_ck_tstsel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_fqmtr_ck_tstsel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_rtcdet_ck_tstsel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_rtc_eosc32_ck_tstsel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_eosc_cali_test_ck_tstsel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_rtc26m_ck_tstsel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_rtc32k_ck_tstsel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_fg_ck_tstsel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_fgadc_ana_ck_tstsel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_bgr_test_ck_tstsel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_pmu75k_ck_tstsel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_smps_ck_tstsel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_aud_ck_tstsel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audif_ck_tstsel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_aud26m_ck_tstsel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vow12m_ck_tstsel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_clksq_en_aud(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_clksq_en_aud(
	void);
extern unsigned int mt6355_upmu_set_rg_clksq_en_fqr(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_clksq_en_fqr(
	void);
extern unsigned int mt6355_upmu_set_rg_clksq_en_aux_ap(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_clksq_en_aux_ap(
	void);
extern unsigned int mt6355_upmu_set_rg_clksq_en_aux_md(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_clksq_en_aux_md(
	void);
extern unsigned int mt6355_upmu_set_rg_clksq_en_aux_gps(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_clksq_en_aux_gps(
	void);
extern unsigned int mt6355_upmu_set_rg_clksq_en_aux_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_clksq_en_aux_rsv(
	void);
extern unsigned int mt6355_upmu_set_rg_clksq_en_aux_ap_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_clksq_en_aux_ap_mode(
	void);
extern unsigned int mt6355_upmu_set_rg_clksq_en_aux_md_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_clksq_en_aux_md_mode(
	void);
extern unsigned int mt6355_upmu_set_rg_clksq_in_sel_va18(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_clksq_in_sel_va18_swctrl(
	unsigned int val);
extern unsigned int mt6355_upmu_set_top_clksq_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_clksq_en_va18(
	void);
extern unsigned int mt6355_upmu_set_rg_clksq_rtc_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_clksq_rtc_en(
	void);
extern unsigned int mt6355_upmu_set_rg_clksq_rtc_en_hw_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_clksq_rtc_en_hw_mode(
	void);
extern unsigned int mt6355_upmu_set_top_clksq_rtc_rsv0(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_enbb_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_xosc_en_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_xosc_en_sel(
	void);
extern unsigned int mt6355_upmu_set_top_clksq_rtc_rsv1(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_clksq_en_vdig18(
	void);
extern unsigned int mt6355_upmu_set_rg_osc_75k_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_osc_75k_trim_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_osc_75k_trim_en(
	void);
extern unsigned int mt6355_upmu_set_rg_osc_75k_trim_rate(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_osc_75k_trim(
	void);
extern unsigned int mt6355_upmu_set_rg_g_smps_ck_pdn_srclken0_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_g_smps_ck_pdn_srclken0_en(
	void);
extern unsigned int mt6355_upmu_set_rg_g_smps_ck_pdn_srclken1_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_g_smps_ck_pdn_srclken1_en(
	void);
extern unsigned int mt6355_upmu_set_rg_g_smps_ck_pdn_srclken2_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_g_smps_ck_pdn_srclken2_en(
	void);
extern unsigned int mt6355_upmu_set_rg_g_smps_ck_pdn_buck_osc_sel_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_g_smps_ck_pdn_buck_osc_sel_en(
	void);
extern unsigned int mt6355_upmu_set_rg_g_smps_ck_pdn_vowen_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_g_smps_ck_pdn_vowen_en(
	void);
extern unsigned int mt6355_upmu_set_rg_osc_sel_srclken0_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_osc_sel_srclken0_en(
	void);
extern unsigned int mt6355_upmu_set_rg_osc_sel_srclken1_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_osc_sel_srclken1_en(
	void);
extern unsigned int mt6355_upmu_set_rg_osc_sel_srclken2_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_osc_sel_srclken2_en(
	void);
extern unsigned int mt6355_upmu_set_rg_osc_sel_buck_ldo_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_osc_sel_buck_ldo_en(
	void);
extern unsigned int mt6355_upmu_set_rg_osc_sel_vowen_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_osc_sel_vowen_en(
	void);
extern unsigned int mt6355_upmu_set_rg_clk_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc2_ckmux_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vproc2_ckmux_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vpa_sw_pdn_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vpa_sw_pdn_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_1m_pdn_w_osc_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_1m_pdn_w_osc_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_clkctl_rsv0(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_dcxo_pwrkey_rstb_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_efuse_man_rst(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_auxadc_rst(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_auxadc_reg_rst(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audio_rst(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_accdet_rst(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_bif_rst(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_driver_rst(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_fgadc_rst(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_fqmtr_rst(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_rtc_rst(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_type_c_cc_rst(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_chrwdt_rst(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_zcd_rst(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audncp_rst(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_clk_trim_rst(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_srclken_rst(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_strup_long_press_rst(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_prot_pmpp_rst(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_spk_rst(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_chrdet_rst(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_ldo_ft_testmode_rst(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_baton_rst(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_fgadc_rst_src_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_cali_rst(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_pseq_pwrmsk_rst_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_top_rst_con1_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_chr_ldo_det_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_chr_ldo_det_sw(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_chrwdt_flag_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_chrwdt_flag_sw(
	unsigned int val);
extern unsigned int mt6355_upmu_set_top_rst_con2_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_wdtrstb_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_wdtrstb_en(
	void);
extern unsigned int mt6355_upmu_set_rg_wdtrstb_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_get_wdtrstb_status(
	void);
extern unsigned int mt6355_upmu_set_wdtrstb_status_clr(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_wdtrstb_fb_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_wdtrstb_fb_en(
	void);
extern unsigned int mt6355_upmu_set_rg_wdtrstb_deb(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_homekey_rst_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_homekey_rst_en(
	void);
extern unsigned int mt6355_upmu_set_rg_pwrkey_rst_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_pwrkey_rst_en(
	void);
extern unsigned int mt6355_upmu_set_rg_pwrrst_tmr_dis(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_pwrkey_rst_td(
	unsigned int val);
extern unsigned int mt6355_upmu_set_top_rst_misc_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_vpwrin_rstb_status(
	unsigned int val);
extern unsigned int mt6355_upmu_set_ddlo_rstb_status(
	unsigned int val);
extern unsigned int mt6355_upmu_set_uvlo_rstb_status(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rtc_ddlo_rstb_status(
	unsigned int val);
extern unsigned int mt6355_upmu_set_chrwdt_reg_rstb_status(
	unsigned int val);
extern unsigned int mt6355_upmu_set_chrdet_reg_rstb_status(
	unsigned int val);
extern unsigned int mt6355_upmu_set_bwdt_ddlo_rstb_status(
	unsigned int val);
extern unsigned int mt6355_upmu_set_top_rst_status_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_top_rst_rsv_con0(
	unsigned int val);
extern unsigned int mt6355_upmu_set_top_rst_rsv_con1(
	unsigned int val);
extern unsigned int mt6355_upmu_set_bank_fqmtr_swrst(
	unsigned int val);
extern unsigned int mt6355_upmu_set_bank_spi_swrst(
	unsigned int val);
extern unsigned int mt6355_upmu_set_bank_strup_swrst(
	unsigned int val);
extern unsigned int mt6355_upmu_set_bank_buck_swrst(
	unsigned int val);
extern unsigned int mt6355_upmu_set_bank_buck_ana_swrst(
	unsigned int val);
extern unsigned int mt6355_upmu_set_bank_wdtdbg_swrst(
	unsigned int val);
extern unsigned int mt6355_upmu_set_bank_ldo_0_swrst(
	unsigned int val);
extern unsigned int mt6355_upmu_set_bank_ldo_1_swrst(
	unsigned int val);
extern unsigned int mt6355_upmu_set_bank_ldo_ana_swrst(
	unsigned int val);
extern unsigned int mt6355_upmu_set_bank_accdet_swrst(
	unsigned int val);
extern unsigned int mt6355_upmu_set_bank_efuse_swrst(
	unsigned int val);
extern unsigned int mt6355_upmu_set_bank_dcxo_swrst(
	unsigned int val);
extern unsigned int mt6355_upmu_set_bank_pchr_swrst(
	unsigned int val);
extern unsigned int mt6355_upmu_set_bank_gpio_swrst(
	unsigned int val);
extern unsigned int mt6355_upmu_set_bank_eosc_cali_swrst(
	unsigned int val);
extern unsigned int mt6355_upmu_set_bank_vrtc_pwm_swrst(
	unsigned int val);
extern unsigned int mt6355_upmu_set_bank_rtc_swrst(
	unsigned int val);
extern unsigned int mt6355_upmu_set_bank_rtc_sec_swrst(
	unsigned int val);
extern unsigned int mt6355_upmu_set_bank_bif_swrst(
	unsigned int val);
extern unsigned int mt6355_upmu_set_bank_fgadc_swrst(
	unsigned int val);
extern unsigned int mt6355_upmu_set_bank_auxadc_swrst(
	unsigned int val);
extern unsigned int mt6355_upmu_set_bank_driver_swrst(
	unsigned int val);
extern unsigned int mt6355_upmu_set_bank_audio_swrst(
	unsigned int val);
extern unsigned int mt6355_upmu_set_bank_audzcd_swrst(
	unsigned int val);
extern unsigned int mt6355_upmu_set_top_rst_bank_con1_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_en_pwrkey(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_pwrkey(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_homekey(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_homekey(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_pwrkey_r(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_pwrkey_r(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_homekey_r(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_homekey_r(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_ni_lbat_int(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_ni_lbat_int(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_chrdet(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_chrdet(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_chrdet_edge(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_chrdet_edge(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_baton_lv(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_baton_lv(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_baton_hv(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_baton_hv(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_baton_bat_in(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_baton_bat_in(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_baton_bat_out(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_baton_bat_out(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_rtc(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_rtc(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_rtc_nsec(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_rtc_nsec(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_bif(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_bif(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_vcdt_hv_det(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_vcdt_hv_det(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_thr_h(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_thr_h(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_thr_l(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_thr_l(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_bat_h(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_bat_h(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_bat_l(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_bat_l(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_bat2_h(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_bat2_h(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_bat2_l(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_bat2_l(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_bat_temp_h(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_bat_temp_h(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_bat_temp_l(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_bat_temp_l(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_auxadc_imp(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_auxadc_imp(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_nag_c_dltv(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_nag_c_dltv(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_jeita_hot(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_jeita_hot(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_jeita_warm(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_jeita_warm(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_jeita_cool(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_jeita_cool(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_jeita_cold(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_jeita_cold(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_vproc11_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_vproc11_oc(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_vproc12_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_vproc12_oc(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_vcore_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_vcore_oc(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_vgpu_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_vgpu_oc(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_vdram1_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_vdram1_oc(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_vdram2_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_vdram2_oc(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_vmodem_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_vmodem_oc(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_vs1_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_vs1_oc(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_vs2_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_vs2_oc(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_vpa_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_vpa_oc(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_vcore_preoc(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_vcore_preoc(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_va10_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_va10_oc(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_va12_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_va12_oc(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_va18_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_va18_oc(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_vbif28_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_vbif28_oc(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_vcama1_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_vcama1_oc(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_vcama2_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_vcama2_oc(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_vxo18_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_vxo18_oc(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_vcamd1_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_vcamd1_oc(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_vcamd2_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_vcamd2_oc(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_vcamio_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_vcamio_oc(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_vcn18_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_vcn18_oc(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_vcn28_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_vcn28_oc(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_vcn33_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_vcn33_oc(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_vtcxo24_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_vtcxo24_oc(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_vemc_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_vemc_oc(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_vfe28_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_vfe28_oc(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_vgp_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_vgp_oc(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_vldo28_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_vldo28_oc(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_vio18_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_vio18_oc(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_vio28_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_vio28_oc(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_vmc_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_vmc_oc(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_vmch_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_vmch_oc(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_vmipi_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_vmipi_oc(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_vrf12_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_vrf12_oc(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_vrf18_1_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_vrf18_1_oc(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_vrf18_2_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_vrf18_2_oc(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_vsim1_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_vsim1_oc(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_vsim2_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_vsim2_oc(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_vgp2_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_vgp2_oc(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_vsram_core_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_vsram_core_oc(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_vsram_proc_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_vsram_proc_oc(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_vsram_gpu_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_vsram_gpu_oc(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_vsram_md_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_vsram_md_oc(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_vufs18_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_vufs18_oc(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_vusb33_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_vusb33_oc(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_vxo22_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_vxo22_oc(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_fg_bat0_h(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_fg_bat0_h(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_fg_bat0_l(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_fg_bat0_l(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_fg_cur_h(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_fg_cur_h(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_fg_cur_l(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_fg_cur_l(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_fg_zcv(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_fg_zcv(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_fg_bat1_h(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_fg_bat1_h(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_fg_bat1_l(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_fg_bat1_l(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_fg_n_charge_l(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_fg_n_charge_l(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_fg_iavg_h(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_fg_iavg_h(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_fg_iavg_l(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_fg_iavg_l(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_fg_time_h(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_fg_time_h(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_fg_discharge(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_fg_discharge(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_fg_charge(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_fg_charge(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_con5(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_con5(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_audio(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_audio(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_mad(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_mad(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_eint_rtc32k_1v8_1(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_eint_rtc32k_1v8_1(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_eint_aud_clk(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_eint_aud_clk(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_eint_aud_dat_mosi(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_eint_aud_dat_mosi(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_eint_aud_dat_miso(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_eint_aud_dat_miso(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_eint_vow_clk_miso(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_eint_vow_clk_miso(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_accdet(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_accdet(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_accdet_eint(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_accdet_eint(
	void);
extern unsigned int mt6355_upmu_set_rg_int_en_spi_cmd_alert(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_en_spi_cmd_alert(
	void);
extern unsigned int mt6355_upmu_set_rg_int_mask_pwrkey(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_homekey(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_pwrkey_r(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_homekey_r(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_ni_lbat_int(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_chrdet(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_chrdet_edge(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_baton_lv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_baton_hv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_baton_bat_in(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_baton_bat_out(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_rtc(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_rtc_nsec(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_bif(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_vcdt_hv_det(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_thr_h(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_thr_l(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_bat_h(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_bat_l(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_bat2_h(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_bat2_l(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_bat_temp_h(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_bat_temp_l(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_auxadc_imp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_nag_c_dltv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_jeita_hot(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_jeita_warm(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_jeita_cool(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_jeita_cold(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_vproc11_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_vproc12_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_vcore_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_vgpu_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_vdram1_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_vdram2_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_vmodem_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_vs1_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_vs2_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_vpa_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_vcore_preoc(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_va10_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_va12_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_va18_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_vbif28_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_vcama1_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_vcama2_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_vxo18_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_vcamd1_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_vcamd2_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_vcamio_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_vcn18_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_vcn28_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_vcn33_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_vtcxo24_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_vemc_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_vfe28_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_vgp_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_vldo28_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_vio18_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_vio28_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_vmc_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_vmch_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_vmipi_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_vrf12_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_vrf18_1_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_vrf18_2_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_vsim1_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_vsim2_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_vgp2_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_vsram_core_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_vsram_proc_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_vsram_gpu_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_vsram_md_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_vufs18_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_vusb33_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_vxo22_oc(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_fg_bat0_h(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_fg_bat0_l(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_fg_cur_h(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_fg_cur_l(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_fg_zcv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_fg_bat1_h(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_fg_bat1_l(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_fg_n_charge_l(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_fg_iavg_h(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_fg_iavg_l(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_fg_time_h(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_fg_discharge(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_fg_charge(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_con5(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_audio(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_mad(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_eint_rtc32k_1v8_1(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_eint_aud_clk(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_eint_aud_dat_mosi(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_eint_aud_dat_miso(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_eint_vow_clk_miso(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_accdet(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_accdet_eint(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_mask_spi_cmd_alert(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_int_status_pwrkey(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_homekey(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_pwrkey_r(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_homekey_r(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_ni_lbat_int(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_chrdet(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_chrdet_edge(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_baton_lv(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_baton_hv(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_baton_bat_in(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_baton_bat_out(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_rtc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_rtc_nsec(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_bif(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_vcdt_hv_det(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_thr_h(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_thr_l(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_bat_h(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_bat_l(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_bat2_h(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_bat2_l(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_bat_temp_h(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_bat_temp_l(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_auxadc_imp(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_nag_c_dltv(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_jeita_hot(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_jeita_warm(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_jeita_cool(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_jeita_cold(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_vproc11_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_vproc12_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_vcore_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_vgpu_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_vdram1_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_vdram2_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_vmodem_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_vs1_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_vs2_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_vpa_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_vcore_preoc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_va10_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_va12_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_va18_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_vbif28_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_vcama1_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_vcama2_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_vxo18_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_vcamd1_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_vcamd2_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_vcamio_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_vcn18_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_vcn28_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_vcn33_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_vtcxo24_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_vemc_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_vfe28_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_vgp_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_vldo28_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_vio18_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_vio28_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_vmc_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_vmch_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_vmipi_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_vrf12_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_vrf18_1_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_vrf18_2_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_vsim1_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_vsim2_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_vgp2_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_vsram_core_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_vsram_proc_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_vsram_gpu_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_vsram_md_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_vufs18_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_vusb33_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_vxo22_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_fg_bat0_h(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_fg_bat0_l(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_fg_cur_h(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_fg_cur_l(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_fg_zcv(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_fg_bat1_h(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_fg_bat1_l(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_fg_n_charge_l(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_fg_iavg_h(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_fg_iavg_l(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_fg_time_h(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_fg_discharge(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_fg_charge(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_con5(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_audio(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_mad(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_eint_rtc32k_1v8_1(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_eint_aud_clk(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_eint_aud_dat_mosi(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_eint_aud_dat_miso(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_eint_vow_clk_miso(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_accdet(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_accdet_eint(
	void);
extern unsigned int mt6355_upmu_get_rg_int_status_spi_cmd_alert(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_pwrkey(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_homekey(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_pwrkey_r(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_homekey_r(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_ni_lbat_int(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_chrdet(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_chrdet_edge(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_baton_lv(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_baton_hv(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_baton_bat_in(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_baton_bat_out(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_rtc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_rtc_nsec(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_bif(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_vcdt_hv_det(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_thr_h(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_thr_l(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_bat_h(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_bat_l(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_bat2_h(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_bat2_l(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_bat_temp_h(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_bat_temp_l(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_auxadc_imp(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_nag_c_dltv(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_jeita_hot(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_jeita_warm(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_jeita_cool(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_jeita_cold(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_vproc11_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_vproc12_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_vcore_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_vgpu_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_vdram1_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_vdram2_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_vmodem_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_vs1_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_vs2_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_vpa_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_vcore_preoc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_va10_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_va12_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_va18_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_vbif28_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_vcama1_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_vcama2_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_vxo18_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_vcamd1_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_vcamd2_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_vcamio_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_vcn18_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_vcn28_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_vcn33_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_vtcxo24_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_vemc_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_vfe28_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_vgp_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_vldo28_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_vio18_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_vio28_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_vmc_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_vmch_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_vmipi_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_vrf12_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_vrf18_1_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_vrf18_2_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_vsim1_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_vsim2_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_vgp2_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_vsram_core_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_vsram_proc_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_vsram_gpu_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_vsram_md_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_vufs18_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_vusb33_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_vxo22_oc(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_fg_bat0_h(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_fg_bat0_l(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_fg_cur_h(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_fg_cur_l(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_fg_zcv(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_fg_bat1_h(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_fg_bat1_l(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_fg_n_charge_l(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_fg_iavg_h(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_fg_iavg_l(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_fg_time_h(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_fg_discharge(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_fg_charge(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_con5(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_audio(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_mad(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_eint_rtc32k_1v8_1(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_eint_aud_clk(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_eint_aud_dat_mosi(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_eint_aud_dat_miso(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_eint_vow_clk_miso(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_accdet(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_accdet_eint(
	void);
extern unsigned int mt6355_upmu_get_rg_int_raw_status_spi_cmd_alert(
	void);
extern unsigned int mt6355_upmu_set_polarity(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_homekey_int_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_pwrkey_int_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_chrdet_int_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_pchr_cm_vinc_polarity_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_pchr_cm_vdec_polarity_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_sen_eint_rtc32k_1v8_1(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_sen_eint_aud_clk(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_sen_eint_aud_dat_mosi(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_sen_eint_aud_dat_miso(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_sen_eint_vow_clk_miso(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_pol_eint_rtc32k_1v8_1(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_pol_eint_aud_clk(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_pol_eint_aud_dat_mosi(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_pol_eint_aud_dat_miso(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_int_pol_eint_vow_clk_miso(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_sw_sel_eint_rtc32k_1v8_1(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_sw_sel_eint_aud_clk(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_sw_sel_eint_aud_dat_mosi(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_sw_sel_eint_aud_dat_miso(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_sw_sel_eint_vow_clk_miso(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_sw_eint_rtc32k_1v8_1(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_sw_eint_aud_clk(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_sw_eint_aud_dat_mosi(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_sw_eint_aud_dat_miso(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_sw_eint_vow_clk_miso(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_deb_eint_rtc32k_1v8_1(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_deb_eint_aud_clk(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_deb_eint_aud_dat_mosi(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_deb_eint_aud_dat_miso(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_deb_eint_vow_clk_miso(
	unsigned int val);
extern unsigned int mt6355_upmu_set_fqmtr_tcksel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_fqmtr_busy(
	void);
extern unsigned int mt6355_upmu_set_fqmtr_dcxo26m_en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_fqmtr_en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_fqmtr_winset(
	unsigned int val);
extern unsigned int mt6355_upmu_get_fqmtr_data(
	void);
extern unsigned int mt6355_upmu_set_rg_slp_rw_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_slp_rw_en(
	void);
extern unsigned int mt6355_upmu_set_rg_spi_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_dew_dio_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_dew_read_test(
	void);
extern unsigned int mt6355_upmu_set_dew_write_test(
	unsigned int val);
extern unsigned int mt6355_upmu_set_dew_crc_swrst(
	unsigned int val);
extern unsigned int mt6355_upmu_set_dew_crc_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_dew_crc_val(
	void);
extern unsigned int mt6355_upmu_set_dew_dbg_mon_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_dew_cipher_key_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_dew_cipher_iv_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_dew_cipher_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_dew_cipher_rdy(
	void);
extern unsigned int mt6355_upmu_set_dew_cipher_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_dew_cipher_swrst(
	unsigned int val);
extern unsigned int mt6355_upmu_set_dew_rddmy_no(
	unsigned int val);
extern unsigned int mt6355_upmu_set_int_type_con0(
	unsigned int val);
extern unsigned int mt6355_upmu_set_int_type_con1(
	unsigned int val);
extern unsigned int mt6355_upmu_set_int_type_con2(
	unsigned int val);
extern unsigned int mt6355_upmu_set_int_type_con3(
	unsigned int val);
extern unsigned int mt6355_upmu_set_int_type_con4(
	unsigned int val);
extern unsigned int mt6355_upmu_set_int_type_con5(
	unsigned int val);
extern unsigned int mt6355_upmu_set_int_type_con6(
	unsigned int val);
extern unsigned int mt6355_upmu_get_cpu_int_sta(
	void);
extern unsigned int mt6355_upmu_get_md32_int_sta(
	void);
extern unsigned int mt6355_upmu_set_rg_srclken_in3_smps_clk_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_srclken_in3_en_smps_test(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_srclken_in3_en_smps_test(
	void);
extern unsigned int mt6355_upmu_set_rg_srclken_in2_smps_clk_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_srclken_in2_en_smps_test(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_srclken_in2_en_smps_test(
	void);
extern unsigned int mt6355_upmu_set_rg_spi_dly_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_record_cmd0(
	void);
extern unsigned int mt6355_upmu_get_record_cmd1(
	void);
extern unsigned int mt6355_upmu_get_record_cmd2(
	void);
extern unsigned int mt6355_upmu_get_record_wdata0(
	void);
extern unsigned int mt6355_upmu_get_record_wdata1(
	void);
extern unsigned int mt6355_upmu_get_record_wdata2(
	void);
extern unsigned int mt6355_upmu_set_rg_addr_target(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_addr_mask(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_wdata_target(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_wdata_mask(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_spi_record_clr(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_cmd_alert_clr(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_thr_det_dis(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_thr_test(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_strup_ther_deb_rmax(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_strup_ther_deb_fmax(
	unsigned int val);
extern unsigned int mt6355_upmu_set_dduvlo_deb_en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_strup_osc_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_strup_osc_en(
	void);
extern unsigned int mt6355_upmu_set_rg_strup_osc_en_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_strup_osc_en_sel(
	void);
extern unsigned int mt6355_upmu_set_rg_strup_ft_ctrl(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_strup_pwron_force(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_biasgen_force(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_strup_pwron(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_strup_pwron_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_biasgen(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_biasgen_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_rtc_xosc32_enb(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_rtc_xosc32_enb(
	void);
extern unsigned int mt6355_upmu_set_rg_rtc_xosc32_enb_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_rtc_xosc32_enb_sel(
	void);
extern unsigned int mt6355_upmu_set_strup_dig_io_pg_force(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_clr_just_smart_rst(
	unsigned int val);
extern unsigned int mt6355_upmu_set_clr_just_rst(
	unsigned int val);
extern unsigned int mt6355_upmu_set_uvlo_l2h_deb_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_just_smart_rst(
	void);
extern unsigned int mt6355_upmu_get_just_pwrkey_rst(
	void);
extern unsigned int mt6355_upmu_get_da_qi_osc_en(
	void);
extern unsigned int mt6355_upmu_set_rg_strup_ext_pmic_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_strup_ext_pmic_en(
	void);
extern unsigned int mt6355_upmu_set_rg_strup_ext_pmic_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_strup_con8_rsv0(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_ext_pmic_en(
	void);
extern unsigned int mt6355_upmu_set_rg_strup_auxadc_start_sw(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_strup_auxadc_rstb_sw(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_strup_auxadc_start_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_strup_auxadc_rstb_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_strup_auxadc_rpcnt_max(
	unsigned int val);
extern unsigned int mt6355_upmu_set_strup_pwroff_seq_en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_strup_pwroff_preoff_en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_strup_dig0_rsv0(
	unsigned int val);
extern unsigned int mt6355_upmu_set_strup_dig1_rsv0(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_rsv_swreg(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_strup_uvlo_u1u2_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_strup_uvlo_u1u2_sel_swctrl(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_strup_thr_clr(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_strup_long_press_ext_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_strup_long_press_ext_td(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_strup_long_press_ext_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_strup_long_press_ext_en(
	void);
extern unsigned int mt6355_upmu_set_rg_strup_long_press_ext_chr_ctrl(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_strup_long_press_ext_pwrkey_ctrl(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_strup_long_press_ext_spar_ctrl(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_strup_long_press_ext_rtca_ctrl(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_smart_rst_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_strup_envtem(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_strup_envtem(
	void);
extern unsigned int mt6355_upmu_set_rg_strup_envtem_ctrl(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_strup_envtem_ctrl(
	void);
extern unsigned int mt6355_upmu_set_rg_strup_pwrkey_count_reset(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_strup_va12_pg_h2l_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_strup_va12_pg_h2l_en(
	void);
extern unsigned int mt6355_upmu_set_rg_strup_va10_pg_h2l_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_strup_va10_pg_h2l_en(
	void);
extern unsigned int mt6355_upmu_set_rg_strup_vsram_gpu_pg_h2l_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_strup_vsram_gpu_pg_h2l_en(
	void);
extern unsigned int mt6355_upmu_set_rg_strup_vsram_md_pg_h2l_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_strup_vsram_md_pg_h2l_en(
	void);
extern unsigned int mt6355_upmu_set_rg_strup_vsram_core_pg_h2l_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_strup_vsram_core_pg_h2l_en(
	void);
extern unsigned int mt6355_upmu_set_rg_strup_va18_pg_h2l_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_strup_va18_pg_h2l_en(
	void);
extern unsigned int mt6355_upmu_set_rg_strup_buck_rsv_pg_h2l_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_strup_buck_rsv_pg_h2l_en(
	void);
extern unsigned int mt6355_upmu_set_rg_strup_vdram2_pg_h2l_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_strup_vdram2_pg_h2l_en(
	void);
extern unsigned int mt6355_upmu_set_rg_strup_vdram1_pg_h2l_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_strup_vdram1_pg_h2l_en(
	void);
extern unsigned int mt6355_upmu_set_rg_strup_vproc12_pg_h2l_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_strup_vproc12_pg_h2l_en(
	void);
extern unsigned int mt6355_upmu_set_rg_strup_vproc11_pg_h2l_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_strup_vproc11_pg_h2l_en(
	void);
extern unsigned int mt6355_upmu_set_rg_strup_vs1_pg_h2l_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_strup_vs1_pg_h2l_en(
	void);
extern unsigned int mt6355_upmu_set_rg_strup_vmodem_pg_h2l_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_strup_vmodem_pg_h2l_en(
	void);
extern unsigned int mt6355_upmu_set_rg_strup_vgpu_pg_h2l_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_strup_vgpu_pg_h2l_en(
	void);
extern unsigned int mt6355_upmu_set_rg_strup_vcore_pg_h2l_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_strup_vcore_pg_h2l_en(
	void);
extern unsigned int mt6355_upmu_set_rg_strup_vs2_pg_h2l_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_strup_vs2_pg_h2l_en(
	void);
extern unsigned int mt6355_upmu_set_rg_strup_ext_pmic_pg_h2l_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_strup_ext_pmic_pg_h2l_en(
	void);
extern unsigned int mt6355_upmu_set_rg_strup_vusb33_pg_h2l_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_strup_vusb33_pg_h2l_en(
	void);
extern unsigned int mt6355_upmu_set_rg_strup_vsram_proc_pg_h2l_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_strup_vsram_proc_pg_h2l_en(
	void);
extern unsigned int mt6355_upmu_set_rg_strup_vufs18_pg_h2l_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_strup_vufs18_pg_h2l_en(
	void);
extern unsigned int mt6355_upmu_set_rg_strup_vemc_pg_h2l_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_strup_vemc_pg_h2l_en(
	void);
extern unsigned int mt6355_upmu_set_rg_strup_va12_pg_enb(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_strup_va12_pg_enb(
	void);
extern unsigned int mt6355_upmu_set_rg_strup_va10_pg_enb(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_strup_va10_pg_enb(
	void);
extern unsigned int mt6355_upmu_set_rg_strup_vsram_gpu_pg_enb(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_strup_vsram_gpu_pg_enb(
	void);
extern unsigned int mt6355_upmu_set_rg_strup_vsram_md_pg_enb(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_strup_vsram_md_pg_enb(
	void);
extern unsigned int mt6355_upmu_set_rg_strup_vsram_core_pg_enb(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_strup_vsram_core_pg_enb(
	void);
extern unsigned int mt6355_upmu_set_rg_strup_va18_pg_enb(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_strup_va18_pg_enb(
	void);
extern unsigned int mt6355_upmu_set_rg_strup_buck_rsv_pg_enb(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_strup_buck_rsv_pg_enb(
	void);
extern unsigned int mt6355_upmu_set_rg_strup_vdram2_pg_enb(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_strup_vdram2_pg_enb(
	void);
extern unsigned int mt6355_upmu_set_rg_strup_vdram1_pg_enb(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_strup_vdram1_pg_enb(
	void);
extern unsigned int mt6355_upmu_set_rg_strup_vproc12_pg_enb(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_strup_vproc12_pg_enb(
	void);
extern unsigned int mt6355_upmu_set_rg_strup_vproc11_pg_enb(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_strup_vproc11_pg_enb(
	void);
extern unsigned int mt6355_upmu_set_rg_strup_vs1_pg_enb(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_strup_vs1_pg_enb(
	void);
extern unsigned int mt6355_upmu_set_rg_strup_vmodem_pg_enb(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_strup_vmodem_pg_enb(
	void);
extern unsigned int mt6355_upmu_set_rg_strup_vgpu_pg_enb(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_strup_vgpu_pg_enb(
	void);
extern unsigned int mt6355_upmu_set_rg_strup_vcore_pg_enb(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_strup_vcore_pg_enb(
	void);
extern unsigned int mt6355_upmu_set_rg_strup_vs2_pg_enb(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_strup_vs2_pg_enb(
	void);
extern unsigned int mt6355_upmu_set_rg_strup_ext_pmic_pg_enb(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_strup_ext_pmic_pg_enb(
	void);
extern unsigned int mt6355_upmu_set_rg_strup_vxo18_pg_enb(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_strup_vxo18_pg_enb(
	void);
extern unsigned int mt6355_upmu_set_rg_strup_vxo22_pg_enb(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_strup_vxo22_pg_enb(
	void);
extern unsigned int mt6355_upmu_set_rg_strup_vusb33_pg_enb(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_strup_vusb33_pg_enb(
	void);
extern unsigned int mt6355_upmu_set_rg_strup_vsram_proc_pg_enb(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_strup_vsram_proc_pg_enb(
	void);
extern unsigned int mt6355_upmu_set_rg_strup_vio28_pg_enb(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_strup_vio28_pg_enb(
	void);
extern unsigned int mt6355_upmu_set_rg_strup_vufs18_pg_enb(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_strup_vufs18_pg_enb(
	void);
extern unsigned int mt6355_upmu_set_rg_strup_vemc_pg_enb(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_strup_vemc_pg_enb(
	void);
extern unsigned int mt6355_upmu_set_rg_strup_vio18_pg_enb(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_strup_vio18_pg_enb(
	void);
extern unsigned int mt6355_upmu_set_rg_strup_buck_rsv_oc_enb(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_strup_buck_rsv_oc_enb(
	void);
extern unsigned int mt6355_upmu_set_rg_strup_vdram2_oc_enb(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_strup_vdram2_oc_enb(
	void);
extern unsigned int mt6355_upmu_set_rg_strup_vdram1_oc_enb(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_strup_vdram1_oc_enb(
	void);
extern unsigned int mt6355_upmu_set_rg_strup_vproc12_oc_enb(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_strup_vproc12_oc_enb(
	void);
extern unsigned int mt6355_upmu_set_rg_strup_vproc11_oc_enb(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_strup_vproc11_oc_enb(
	void);
extern unsigned int mt6355_upmu_set_rg_strup_vs1_oc_enb(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_strup_vs1_oc_enb(
	void);
extern unsigned int mt6355_upmu_set_rg_strup_vmodem_oc_enb(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_strup_vmodem_oc_enb(
	void);
extern unsigned int mt6355_upmu_set_rg_strup_vgpu_oc_enb(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_strup_vgpu_oc_enb(
	void);
extern unsigned int mt6355_upmu_set_rg_strup_vcore_oc_enb(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_strup_vcore_oc_enb(
	void);
extern unsigned int mt6355_upmu_set_rg_strup_vs2_oc_enb(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_strup_vs2_oc_enb(
	void);
extern unsigned int mt6355_upmu_set_rg_strup_long_press_reset_extend(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ext_pmic_pg_debtd(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_rtc_spar_deb_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_rtc_spar_deb_en(
	void);
extern unsigned int mt6355_upmu_set_rg_rtc_alarm_deb_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_rtc_alarm_deb_en(
	void);
extern unsigned int mt6355_upmu_set_rg_tm_out(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_thr_loc_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_thrdet_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_strup_thr_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_thr_tmode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vref_bg(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_strup_iref_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_rst_drvsel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_en_drvsel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_pmu_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rgs_ana_chip_id(
	void);
extern unsigned int mt6355_upmu_set_rg_pwrhold(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_usbdl_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_crst(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_wrst(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_rstb_onintv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_crst_intv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_wrst_intv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_pseq_ivgen_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_pseq_fsm_rst_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_pseq_pg_ck_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_pseq_1ms_tk_ext(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_pseq_spar_xcpt_mask(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_pseq_rtca_xcpt_mask(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_wdtrst_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_wdtrst_en(
	void);
extern unsigned int mt6355_upmu_set_rg_wdtrst_act(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_pspg_shdn_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_pspg_shdn_en(
	void);
extern unsigned int mt6355_upmu_set_rg_thm_shdn_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_thm_shdn_en(
	void);
extern unsigned int mt6355_upmu_set_rg_keypwr_vcore_opt(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_pseq_force_on(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_pseq_force_all_doff(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_pseq_f75k_force(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_pseq_rsv0(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_pseq_rsv1(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_pseq_rsv2(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_bwdt_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_bwdt_en(
	void);
extern unsigned int mt6355_upmu_set_rg_bwdt_tsel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_bwdt_csel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_bwdt_td(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_bwdt_chrtd(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_bwdt_ddlo_td(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_bwdt_srcsel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_cps_w_key(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_slot_intv_up(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_seq_len(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_slot_intv_down(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_dseq_len(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vs2_usa(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vsram_core_usa(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vsram_md_usa(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vsram_gpu_usa(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcore_usa(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vgpu_usa(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vmodem_usa(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vs1_usa(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_va10_usa(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_va12_usa(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vio18_usa(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vemc_usa(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vufs18_usa(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vio28_usa(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vsram_proc_usa(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc11_usa(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc12_usa(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ext_pmic_usa(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vdram1_usa(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vdram2_usa(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vusb33_usa(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vxo22_usa(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vxo18_usa(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_rsv_usa(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vs2_dsa(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vsram_core_dsa(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vsram_md_dsa(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vsram_gpu_dsa(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcore_dsa(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vgpu_dsa(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vmodem_dsa(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vs1_dsa(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_va10_dsa(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_va12_dsa(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vio18_dsa(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vemc_dsa(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vufs18_dsa(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vio28_dsa(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vsram_proc_dsa(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc11_dsa(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc12_dsa(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ext_pmic_dsa(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vdram1_dsa(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vdram2_dsa(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vusb33_dsa(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vxo22_dsa(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vxo18_dsa(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_rsv_dsa(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_por_flag(
	unsigned int val);
extern unsigned int mt6355_upmu_get_sts_pwrkey(
	void);
extern unsigned int mt6355_upmu_get_sts_rtca(
	void);
extern unsigned int mt6355_upmu_get_sts_chrin(
	void);
extern unsigned int mt6355_upmu_get_sts_spar(
	void);
extern unsigned int mt6355_upmu_get_sts_rboot(
	void);
extern unsigned int mt6355_upmu_get_sts_uvlo(
	void);
extern unsigned int mt6355_upmu_get_sts_pgfail(
	void);
extern unsigned int mt6355_upmu_get_sts_psoc(
	void);
extern unsigned int mt6355_upmu_get_sts_thrdn(
	void);
extern unsigned int mt6355_upmu_get_sts_wrst(
	void);
extern unsigned int mt6355_upmu_get_sts_crst(
	void);
extern unsigned int mt6355_upmu_get_sts_pkeylp(
	void);
extern unsigned int mt6355_upmu_get_sts_normoff(
	void);
extern unsigned int mt6355_upmu_get_sts_bwdt(
	void);
extern unsigned int mt6355_upmu_get_sts_ddlo(
	void);
extern unsigned int mt6355_upmu_get_sts_wdt(
	void);
extern unsigned int mt6355_upmu_get_sts_pupsrc(
	void);
extern unsigned int mt6355_upmu_get_sts_keypwr(
	void);
extern unsigned int mt6355_upmu_set_rg_poffsts_clr(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ponsts_clr(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_ldo_ft_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_ldo_ft_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_dcm_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_all_con0_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_stb_max(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_lp_prot_disable(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vsleep_src0(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vsleep_src1(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_r2r_src0(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_r2r_src1(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_lp_seq_count(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_on_seq_count(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_minfreq_latency_max(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_minfreq_duration_max(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vproc11_oc_sdn_status(
	void);
extern unsigned int mt6355_upmu_get_rg_buck_vproc12_oc_sdn_status(
	void);
extern unsigned int mt6355_upmu_get_rg_buck_vcore_oc_sdn_status(
	void);
extern unsigned int mt6355_upmu_get_rg_buck_vgpu_oc_sdn_status(
	void);
extern unsigned int mt6355_upmu_get_rg_buck_vdram1_oc_sdn_status(
	void);
extern unsigned int mt6355_upmu_get_rg_buck_vdram2_oc_sdn_status(
	void);
extern unsigned int mt6355_upmu_get_rg_buck_vmodem_oc_sdn_status(
	void);
extern unsigned int mt6355_upmu_get_rg_buck_vs1_oc_sdn_status(
	void);
extern unsigned int mt6355_upmu_get_rg_buck_vs2_oc_sdn_status(
	void);
extern unsigned int mt6355_upmu_get_rg_buck_vpa_oc_sdn_status(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vproc11_oc_sdn_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vproc11_oc_sdn_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vproc12_oc_sdn_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vproc12_oc_sdn_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vcore_oc_sdn_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vcore_oc_sdn_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vgpu_oc_sdn_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vgpu_oc_sdn_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vdram1_oc_sdn_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vdram1_oc_sdn_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vdram2_oc_sdn_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vdram2_oc_sdn_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vmodem_oc_sdn_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vmodem_oc_sdn_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vs1_oc_sdn_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vs1_oc_sdn_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vs2_oc_sdn_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vs2_oc_sdn_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vpa_oc_sdn_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vpa_oc_sdn_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_k_rst_done(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_k_map_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_k_once_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_k_once_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_k_once(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_k_start_manual(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_k_src_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_k_auto_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_k_auto_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_k_inv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_k_ck_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_k_ck_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_k_control_smps(
	unsigned int val);
extern unsigned int mt6355_upmu_get_buck_k_result(
	void);
extern unsigned int mt6355_upmu_get_buck_k_done(
	void);
extern unsigned int mt6355_upmu_get_buck_k_control(
	void);
extern unsigned int mt6355_upmu_get_da_qi_smps_osc_cal(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_k_buck_ck_cnt(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vow_buck_vcore_dvs_done(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vow_ldo_vsram_core_dvs_done(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vow_buck_vcore_dvs_sw_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vow_ldo_vsram_core_dvs_sw_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_get_vow_buck_vcore_dvs_done(
	void);
extern unsigned int mt6355_upmu_get_vow_ldo_vsram_core_dvs_done(
	void);
extern unsigned int mt6355_upmu_get_vow_dvs_done(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vproc11_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vproc11_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vproc11_lp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vproc11_vosel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vproc11_vosel(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vproc11_vosel_sleep(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vproc11_vosel_sleep(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vproc11_sfchg_frate(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vproc11_sfchg_fen(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vproc11_sfchg_rrate(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vproc11_sfchg_ren(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vproc11_dvs_en_td(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vproc11_dvs_en_td(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vproc11_dvs_en_ctrl(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vproc11_dvs_en_ctrl(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vproc11_dvs_en_once(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vproc11_dvs_en_once(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vproc11_dvs_down_td(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vproc11_dvs_down_ctrl(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vproc11_dvs_down_once(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vproc11_sw_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vproc11_sw_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vproc11_hw0_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vproc11_hw0_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vproc11_hw1_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vproc11_hw1_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vproc11_hw2_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vproc11_hw2_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vproc11_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vproc11_hw0_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vproc11_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vproc11_hw1_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vproc11_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vproc11_hw2_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vproc11_on_op(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vproc11_lp_op(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vproc11_sp_sw_vosel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vproc11_sp_sw_vosel(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vproc11_sp_sw_vosel_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vproc11_sp_sw_vosel_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vproc11_sp_on_vosel_mux_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vproc11_sp_on_vosel_mux_sel(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vproc11_oc_deg_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vproc11_oc_deg_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vproc11_oc_wnd(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vproc11_oc_thd(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_vproc11_vosel(
	void);
extern unsigned int mt6355_upmu_get_da_vproc11_vosel_gray(
	void);
extern unsigned int mt6355_upmu_get_da_vproc11_en(
	void);
extern unsigned int mt6355_upmu_get_da_vproc11_stb(
	void);
extern unsigned int mt6355_upmu_get_da_vproc11_vsleep_sel(
	void);
extern unsigned int mt6355_upmu_get_da_vproc11_r2r_pdn(
	void);
extern unsigned int mt6355_upmu_get_da_vproc11_dvs_en(
	void);
extern unsigned int mt6355_upmu_get_da_vproc11_dvs_down(
	void);
extern unsigned int mt6355_upmu_get_da_vproc11_ssh(
	void);
extern unsigned int mt6355_upmu_get_da_vproc11_minfreq_discharge(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vproc11_oc_flag_clr_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vproc11_osc_sel_dis(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vproc11_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vproc11_ck_sw_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vproc11_ck_sw_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vproc12_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vproc12_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vproc12_lp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vproc12_vosel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vproc12_vosel(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vproc12_vosel_sleep(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vproc12_vosel_sleep(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vproc12_sfchg_frate(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vproc12_sfchg_fen(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vproc12_sfchg_rrate(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vproc12_sfchg_ren(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vproc12_dvs_en_td(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vproc12_dvs_en_td(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vproc12_dvs_en_ctrl(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vproc12_dvs_en_ctrl(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vproc12_dvs_en_once(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vproc12_dvs_en_once(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vproc12_dvs_down_td(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vproc12_dvs_down_ctrl(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vproc12_dvs_down_once(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vproc12_sw_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vproc12_sw_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vproc12_hw0_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vproc12_hw0_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vproc12_hw1_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vproc12_hw1_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vproc12_hw2_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vproc12_hw2_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vproc12_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vproc12_hw0_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vproc12_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vproc12_hw1_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vproc12_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vproc12_hw2_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vproc12_on_op(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vproc12_lp_op(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vproc12_sp_sw_vosel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vproc12_sp_sw_vosel(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vproc12_sp_sw_vosel_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vproc12_sp_sw_vosel_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vproc12_sp_on_vosel_mux_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vproc12_sp_on_vosel_mux_sel(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vproc12_oc_deg_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vproc12_oc_deg_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vproc12_oc_wnd(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vproc12_oc_thd(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_vproc12_vosel(
	void);
extern unsigned int mt6355_upmu_get_da_vproc12_vosel_gray(
	void);
extern unsigned int mt6355_upmu_get_da_vproc12_en(
	void);
extern unsigned int mt6355_upmu_get_da_vproc12_stb(
	void);
extern unsigned int mt6355_upmu_get_da_vproc12_vsleep_sel(
	void);
extern unsigned int mt6355_upmu_get_da_vproc12_r2r_pdn(
	void);
extern unsigned int mt6355_upmu_get_da_vproc12_dvs_en(
	void);
extern unsigned int mt6355_upmu_get_da_vproc12_dvs_down(
	void);
extern unsigned int mt6355_upmu_get_da_vproc12_ssh(
	void);
extern unsigned int mt6355_upmu_get_da_vproc12_minfreq_discharge(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vproc12_oc_flag_clr_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vproc12_osc_sel_dis(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vproc12_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vproc12_ck_sw_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vproc12_ck_sw_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vcore_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vcore_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vcore_lp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vcore_vosel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vcore_vosel(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vcore_vosel_sleep(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vcore_vosel_sleep(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vcore_sfchg_frate(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vcore_sfchg_fen(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vcore_sfchg_rrate(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vcore_sfchg_ren(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vcore_dvs_en_td(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vcore_dvs_en_td(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vcore_dvs_en_ctrl(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vcore_dvs_en_ctrl(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vcore_dvs_en_once(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vcore_dvs_en_once(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vcore_dvs_down_td(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vcore_dvs_down_ctrl(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vcore_dvs_down_once(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vcore_sw_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vcore_sw_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vcore_hw0_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vcore_hw0_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vcore_hw1_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vcore_hw1_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vcore_hw2_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vcore_hw2_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vcore_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vcore_hw0_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vcore_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vcore_hw1_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vcore_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vcore_hw2_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vcore_on_op(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vcore_lp_op(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vcore_sp_sw_vosel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vcore_sp_sw_vosel(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vcore_sp_sw_vosel_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vcore_sp_sw_vosel_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vcore_sp_on_vosel_mux_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vcore_sp_on_vosel_mux_sel(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vcore_oc_deg_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vcore_oc_deg_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vcore_oc_wnd(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vcore_oc_thd(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_vcore_vosel(
	void);
extern unsigned int mt6355_upmu_get_da_vcore_vosel_gray(
	void);
extern unsigned int mt6355_upmu_get_da_vcore_en(
	void);
extern unsigned int mt6355_upmu_get_da_vcore_stb(
	void);
extern unsigned int mt6355_upmu_get_da_vcore_vsleep_sel(
	void);
extern unsigned int mt6355_upmu_get_da_vcore_r2r_pdn(
	void);
extern unsigned int mt6355_upmu_get_da_vcore_dvs_en(
	void);
extern unsigned int mt6355_upmu_get_da_vcore_dvs_down(
	void);
extern unsigned int mt6355_upmu_get_da_vcore_ssh(
	void);
extern unsigned int mt6355_upmu_get_da_vcore_minfreq_discharge(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vcore_oc_flag_clr_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vcore_osc_sel_dis(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vcore_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vcore_ck_sw_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vcore_ck_sw_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vcore_sshub_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vcore_sshub_on(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vcore_sshub_vosel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vcore_sshub_vosel(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vgpu_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vgpu_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vgpu_lp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vgpu_vosel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vgpu_vosel(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vgpu_vosel_sleep(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vgpu_vosel_sleep(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vgpu_sfchg_frate(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vgpu_sfchg_fen(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vgpu_sfchg_rrate(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vgpu_sfchg_ren(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vgpu_dvs_en_td(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vgpu_dvs_en_td(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vgpu_dvs_en_ctrl(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vgpu_dvs_en_ctrl(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vgpu_dvs_en_once(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vgpu_dvs_en_once(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vgpu_dvs_down_td(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vgpu_dvs_down_ctrl(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vgpu_dvs_down_once(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vgpu_sw_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vgpu_sw_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vgpu_hw0_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vgpu_hw0_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vgpu_hw1_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vgpu_hw1_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vgpu_hw2_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vgpu_hw2_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vgpu_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vgpu_hw0_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vgpu_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vgpu_hw1_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vgpu_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vgpu_hw2_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vgpu_on_op(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vgpu_lp_op(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vgpu_sp_sw_vosel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vgpu_sp_sw_vosel(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vgpu_sp_sw_vosel_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vgpu_sp_sw_vosel_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vgpu_sp_on_vosel_mux_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vgpu_sp_on_vosel_mux_sel(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vgpu_oc_deg_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vgpu_oc_deg_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vgpu_oc_wnd(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vgpu_oc_thd(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_vgpu_vosel(
	void);
extern unsigned int mt6355_upmu_get_da_vgpu_vosel_gray(
	void);
extern unsigned int mt6355_upmu_get_da_vgpu_en(
	void);
extern unsigned int mt6355_upmu_get_da_vgpu_stb(
	void);
extern unsigned int mt6355_upmu_get_da_vgpu_vsleep_sel(
	void);
extern unsigned int mt6355_upmu_get_da_vgpu_r2r_pdn(
	void);
extern unsigned int mt6355_upmu_get_da_vgpu_dvs_en(
	void);
extern unsigned int mt6355_upmu_get_da_vgpu_dvs_down(
	void);
extern unsigned int mt6355_upmu_get_da_vgpu_ssh(
	void);
extern unsigned int mt6355_upmu_get_da_vgpu_minfreq_discharge(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vgpu_oc_flag_clr_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vgpu_osc_sel_dis(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vgpu_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vgpu_ck_sw_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vgpu_ck_sw_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vdram1_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vdram1_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vdram1_lp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vdram1_vosel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vdram1_vosel(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vdram1_vosel_sleep(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vdram1_vosel_sleep(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vdram1_sfchg_frate(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vdram1_sfchg_fen(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vdram1_sfchg_rrate(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vdram1_sfchg_ren(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vdram1_dvs_en_td(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vdram1_dvs_en_td(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vdram1_dvs_en_ctrl(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vdram1_dvs_en_ctrl(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vdram1_dvs_en_once(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vdram1_dvs_en_once(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vdram1_dvs_down_td(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vdram1_dvs_down_ctrl(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vdram1_dvs_down_once(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vdram1_sw_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vdram1_sw_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vdram1_hw0_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vdram1_hw0_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vdram1_hw1_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vdram1_hw1_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vdram1_hw2_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vdram1_hw2_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vdram1_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vdram1_hw0_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vdram1_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vdram1_hw1_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vdram1_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vdram1_hw2_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vdram1_on_op(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vdram1_lp_op(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vdram1_sp_sw_vosel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vdram1_sp_sw_vosel(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vdram1_sp_sw_vosel_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vdram1_sp_sw_vosel_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vdram1_sp_on_vosel_mux_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vdram1_sp_on_vosel_mux_sel(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vdram1_oc_deg_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vdram1_oc_deg_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vdram1_oc_wnd(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vdram1_oc_thd(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_vdram1_vosel(
	void);
extern unsigned int mt6355_upmu_get_da_vdram1_vosel_gray(
	void);
extern unsigned int mt6355_upmu_get_da_vdram1_en(
	void);
extern unsigned int mt6355_upmu_get_da_vdram1_stb(
	void);
extern unsigned int mt6355_upmu_get_da_vdram1_vsleep_sel(
	void);
extern unsigned int mt6355_upmu_get_da_vdram1_r2r_pdn(
	void);
extern unsigned int mt6355_upmu_get_da_vdram1_dvs_en(
	void);
extern unsigned int mt6355_upmu_get_da_vdram1_dvs_down(
	void);
extern unsigned int mt6355_upmu_get_da_vdram1_ssh(
	void);
extern unsigned int mt6355_upmu_get_da_vdram1_minfreq_discharge(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vdram1_oc_flag_clr_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vdram1_osc_sel_dis(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vdram1_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vdram1_ck_sw_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vdram1_ck_sw_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vdram2_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vdram2_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vdram2_lp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vdram2_vosel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vdram2_vosel(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vdram2_vosel_sleep(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vdram2_vosel_sleep(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vdram2_sfchg_frate(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vdram2_sfchg_fen(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vdram2_sfchg_rrate(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vdram2_sfchg_ren(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vdram2_dvs_en_td(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vdram2_dvs_en_td(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vdram2_dvs_en_ctrl(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vdram2_dvs_en_ctrl(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vdram2_dvs_en_once(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vdram2_dvs_en_once(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vdram2_dvs_down_td(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vdram2_dvs_down_ctrl(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vdram2_dvs_down_once(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vdram2_sw_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vdram2_sw_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vdram2_hw0_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vdram2_hw0_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vdram2_hw1_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vdram2_hw1_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vdram2_hw2_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vdram2_hw2_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vdram2_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vdram2_hw0_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vdram2_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vdram2_hw1_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vdram2_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vdram2_hw2_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vdram2_on_op(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vdram2_lp_op(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vdram2_sp_sw_vosel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vdram2_sp_sw_vosel(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vdram2_sp_sw_vosel_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vdram2_sp_sw_vosel_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vdram2_sp_on_vosel_mux_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vdram2_sp_on_vosel_mux_sel(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vdram2_oc_deg_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vdram2_oc_deg_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vdram2_oc_wnd(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vdram2_oc_thd(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_vdram2_vosel(
	void);
extern unsigned int mt6355_upmu_get_da_vdram2_vosel_gray(
	void);
extern unsigned int mt6355_upmu_get_da_vdram2_en(
	void);
extern unsigned int mt6355_upmu_get_da_vdram2_stb(
	void);
extern unsigned int mt6355_upmu_get_da_vdram2_vsleep_sel(
	void);
extern unsigned int mt6355_upmu_get_da_vdram2_r2r_pdn(
	void);
extern unsigned int mt6355_upmu_get_da_vdram2_dvs_en(
	void);
extern unsigned int mt6355_upmu_get_da_vdram2_dvs_down(
	void);
extern unsigned int mt6355_upmu_get_da_vdram2_ssh(
	void);
extern unsigned int mt6355_upmu_get_da_vdram2_minfreq_discharge(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vdram2_oc_flag_clr_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vdram2_osc_sel_dis(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vdram2_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vdram2_ck_sw_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vdram2_ck_sw_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vmodem_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vmodem_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vmodem_lp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vmodem_vosel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vmodem_vosel(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vmodem_vosel_sleep(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vmodem_vosel_sleep(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vmodem_sfchg_frate(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vmodem_sfchg_fen(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vmodem_sfchg_rrate(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vmodem_sfchg_ren(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vmodem_dvs_en_td(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vmodem_dvs_en_td(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vmodem_dvs_en_ctrl(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vmodem_dvs_en_ctrl(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vmodem_dvs_en_once(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vmodem_dvs_en_once(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vmodem_dvs_down_td(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vmodem_dvs_down_ctrl(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vmodem_dvs_down_once(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vmodem_sw_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vmodem_sw_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vmodem_hw0_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vmodem_hw0_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vmodem_hw1_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vmodem_hw1_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vmodem_hw2_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vmodem_hw2_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vmodem_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vmodem_hw0_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vmodem_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vmodem_hw1_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vmodem_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vmodem_hw2_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vmodem_on_op(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vmodem_lp_op(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vmodem_sp_sw_vosel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vmodem_sp_sw_vosel(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vmodem_sp_sw_vosel_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vmodem_sp_sw_vosel_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vmodem_sp_on_vosel_mux_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vmodem_sp_on_vosel_mux_sel(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vmodem_oc_deg_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vmodem_oc_deg_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vmodem_oc_wnd(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vmodem_oc_thd(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_vmodem_vosel(
	void);
extern unsigned int mt6355_upmu_get_da_vmodem_vosel_gray(
	void);
extern unsigned int mt6355_upmu_get_da_vmodem_en(
	void);
extern unsigned int mt6355_upmu_get_da_vmodem_stb(
	void);
extern unsigned int mt6355_upmu_get_da_vmodem_vsleep_sel(
	void);
extern unsigned int mt6355_upmu_get_da_vmodem_r2r_pdn(
	void);
extern unsigned int mt6355_upmu_get_da_vmodem_dvs_en(
	void);
extern unsigned int mt6355_upmu_get_da_vmodem_minfreq_discharge(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vmodem_oc_flag_clr_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vmodem_osc_sel_dis(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vmodem_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vmodem_ck_sw_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vmodem_ck_sw_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vmodem_vosel_dlc0(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vmodem_vosel_dlc0(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vmodem_vosel_dlc1(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vmodem_vosel_dlc1(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vmodem_vosel_dlc2(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vmodem_vosel_dlc2(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vmodem_dlc0(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vmodem_dlc1(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vmodem_dlc2(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vmodem_dlc3(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vmodem_dlc_map_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vmodem_dlc_map_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vmodem_dlc(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_vmodem_dlc(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vs1_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vs1_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vs1_lp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vs1_vosel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vs1_vosel(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vs1_vosel_sleep(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vs1_vosel_sleep(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vs1_sfchg_frate(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vs1_sfchg_fen(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vs1_sfchg_rrate(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vs1_sfchg_ren(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vs1_dvs_en_td(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vs1_dvs_en_td(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vs1_dvs_en_ctrl(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vs1_dvs_en_ctrl(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vs1_dvs_en_once(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vs1_dvs_en_once(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vs1_dvs_down_td(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vs1_dvs_down_ctrl(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vs1_dvs_down_once(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vs1_sw_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vs1_sw_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vs1_hw0_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vs1_hw0_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vs1_hw1_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vs1_hw1_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vs1_hw2_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vs1_hw2_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vs1_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vs1_hw0_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vs1_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vs1_hw1_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vs1_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vs1_hw2_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vs1_on_op(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vs1_lp_op(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vs1_sp_sw_vosel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vs1_sp_sw_vosel(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vs1_sp_sw_vosel_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vs1_sp_sw_vosel_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vs1_sp_on_vosel_mux_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vs1_sp_on_vosel_mux_sel(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vs1_oc_deg_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vs1_oc_deg_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vs1_oc_wnd(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vs1_oc_thd(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_vs1_vosel(
	void);
extern unsigned int mt6355_upmu_get_da_vs1_vosel_gray(
	void);
extern unsigned int mt6355_upmu_get_da_vs1_en(
	void);
extern unsigned int mt6355_upmu_get_da_vs1_stb(
	void);
extern unsigned int mt6355_upmu_get_da_vs1_vsleep_sel(
	void);
extern unsigned int mt6355_upmu_get_da_vs1_r2r_pdn(
	void);
extern unsigned int mt6355_upmu_get_da_vs1_dvs_en(
	void);
extern unsigned int mt6355_upmu_get_da_vs1_minfreq_discharge(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vs1_oc_flag_clr_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vs1_osc_sel_dis(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vs1_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vs1_ck_sw_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vs1_ck_sw_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vs1_voter_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vs1_voter_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vs1_voter_vosel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vs1_voter_vosel(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vs2_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vs2_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vs2_lp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vs2_vosel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vs2_vosel(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vs2_vosel_sleep(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vs2_vosel_sleep(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vs2_sfchg_frate(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vs2_sfchg_fen(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vs2_sfchg_rrate(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vs2_sfchg_ren(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vs2_dvs_en_td(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vs2_dvs_en_td(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vs2_dvs_en_ctrl(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vs2_dvs_en_ctrl(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vs2_dvs_en_once(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vs2_dvs_en_once(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vs2_dvs_down_td(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vs2_dvs_down_ctrl(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vs2_dvs_down_once(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vs2_sw_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vs2_sw_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vs2_hw0_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vs2_hw0_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vs2_hw1_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vs2_hw1_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vs2_hw2_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vs2_hw2_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vs2_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vs2_hw0_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vs2_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vs2_hw1_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vs2_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vs2_hw2_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vs2_on_op(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vs2_lp_op(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vs2_sp_sw_vosel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vs2_sp_sw_vosel(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vs2_sp_sw_vosel_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vs2_sp_sw_vosel_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vs2_sp_on_vosel_mux_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vs2_sp_on_vosel_mux_sel(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vs2_oc_deg_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vs2_oc_deg_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vs2_oc_wnd(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vs2_oc_thd(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_vs2_vosel(
	void);
extern unsigned int mt6355_upmu_get_da_vs2_vosel_gray(
	void);
extern unsigned int mt6355_upmu_get_da_vs2_en(
	void);
extern unsigned int mt6355_upmu_get_da_vs2_stb(
	void);
extern unsigned int mt6355_upmu_get_da_vs2_vsleep_sel(
	void);
extern unsigned int mt6355_upmu_get_da_vs2_r2r_pdn(
	void);
extern unsigned int mt6355_upmu_get_da_vs2_dvs_en(
	void);
extern unsigned int mt6355_upmu_get_da_vs2_minfreq_discharge(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vs2_oc_flag_clr_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vs2_osc_sel_dis(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vs2_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vs2_ck_sw_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vs2_ck_sw_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vs2_voter_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vs2_voter_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vs2_voter_vosel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vs2_voter_vosel(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vpa_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vpa_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vpa_vosel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vpa_vosel(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vpa_sfchg_frate(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vpa_sfchg_fen(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vpa_sfchg_rrate(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vpa_sfchg_ren(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vpa_dvs_transt_td(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vpa_dvs_transt_ctrl(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vpa_dvs_transt_once(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vpa_dvs_bw_td(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vpa_dvs_bw_ctrl(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vpa_dvs_bw_once(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vpa_oc_deg_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vpa_oc_deg_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vpa_oc_wnd(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vpa_oc_thd(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_vpa_vosel(
	void);
extern unsigned int mt6355_upmu_get_da_vpa_vosel_gray(
	void);
extern unsigned int mt6355_upmu_get_da_vpa_en(
	void);
extern unsigned int mt6355_upmu_get_da_vpa_stb(
	void);
extern unsigned int mt6355_upmu_get_da_vpa_dvs_transt(
	void);
extern unsigned int mt6355_upmu_get_da_vpa_dvs_bw(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vpa_oc_flag_clr_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vpa_osc_sel_dis(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vpa_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vpa_ck_sw_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vpa_ck_sw_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vpa_vosel_dlc011(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vpa_vosel_dlc011(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vpa_vosel_dlc111(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vpa_vosel_dlc111(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vpa_vosel_dlc001(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vpa_vosel_dlc001(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vpa_dlc_map_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vpa_dlc_map_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vpa_dlc(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_vpa_dlc(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vpa_msfg_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_buck_vpa_msfg_en(
	void);
extern unsigned int mt6355_upmu_set_rg_buck_vpa_msfg_rdelta2go(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vpa_msfg_fdelta2go(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vpa_msfg_rrate0(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vpa_msfg_rrate1(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vpa_msfg_rrate2(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vpa_msfg_rrate3(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vpa_msfg_rrate4(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vpa_msfg_rrate5(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vpa_msfg_rthd0(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vpa_msfg_rthd1(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vpa_msfg_rthd2(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vpa_msfg_rthd3(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vpa_msfg_rthd4(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vpa_msfg_frate0(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vpa_msfg_frate1(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vpa_msfg_frate2(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vpa_msfg_frate3(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vpa_msfg_frate4(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vpa_msfg_frate5(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vpa_msfg_fthd0(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vpa_msfg_fthd1(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vpa_msfg_fthd2(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vpa_msfg_fthd3(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_buck_vpa_msfg_fthd4(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_smps_testmode_b(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vpa_bursth(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vpa_burstl(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vpa_trimh(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vpa_triml(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vpa_trim_ref(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcore_trimh(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcore_triml(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcore_vsleep_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcore_sleep_voltage(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vdram1_trimh(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vdram1_triml(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vdram1_vsleep_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vdram1_sleep_voltage(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vmodem_trimh(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vmodem_triml(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vmodem_vsleep_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vmodem_sleep_voltage(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vgpu_trimh(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vgpu_triml(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vgpu_vsleep_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vgpu_sleep_voltage(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vs1_trimh(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vs1_triml(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vs1_vsleep_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vs1_sleep_voltage(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vs2_trimh(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vs2_triml(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vs2_vsleep_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vs2_sleep_voltage(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vdram2_trimh(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vdram2_triml(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vdram2_vsleep_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vdram2_sleep_voltage(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc11_trimh(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc11_triml(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc11_vsleep_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc11_sleep_voltage(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc12_trimh(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc12_triml(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc12_vsleep_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc12_sleep_voltage(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vsram_proc_trimh(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vsram_proc_triml(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vsram_proc_vsleep_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vsram_proc_sleep_voltage(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vsram_core_trimh(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vsram_core_triml(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vsram_core_vsleep_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vsram_core_sleep_voltage(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vsram_gpu_trimh(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vsram_gpu_triml(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vsram_gpu_vsleep_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vsram_gpu_sleep_voltage(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vsram_md_trimh(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vsram_md_triml(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vsram_md_vsleep_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vsram_md_sleep_voltage(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_smps_ivgd_det(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_voutdet_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_voutdet_en(
	void);
extern unsigned int mt6355_upmu_set_rg_autok_rst(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc11_fpwm(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc12_fpwm(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc11_ndis_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vproc11_ndis_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vproc12_ndis_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vproc12_ndis_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vproc11_fcot(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc12_fcot(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc_tmdl(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc_disconfig20(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc11_tbdis(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc12_tbdis(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc11_vdiffoff(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc12_vdiffoff(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc11_rcomp0(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc11_rcomp1(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc11_ccomp0(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc11_ccomp1(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc11_ramp_slp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc12_rcomp0(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc12_rcomp1(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc12_ccomp0(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc12_ccomp1(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc12_ramp_slp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc11_rcs(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc12_rcs(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc11_rcb(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc12_rcb(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc11_csp_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc12_csp_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc11_csn_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc12_csn_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc11_zc_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc12_zc_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc11_nlim_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc12_nlim_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc11_rpsi1_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc12_rpsi1_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc11_tb_width(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc12_tb_width(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc11_ug_sr(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc11_lg_sr(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc12_ug_sr(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc12_lg_sr(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc11_pfm_ton(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc12_pfm_ton(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc11_ton_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc12_ton_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rgs_vproc11_oc_status(
	void);
extern unsigned int mt6355_upmu_get_rgs_vproc12_oc_status(
	void);
extern unsigned int mt6355_upmu_get_rgs_vproc_trimok_status(
	void);
extern unsigned int mt6355_upmu_get_rgs_vproc_config20_status(
	void);
extern unsigned int mt6355_upmu_get_rgs_vproc11_preoc_status(
	void);
extern unsigned int mt6355_upmu_get_rgs_vproc11_dig_mon(
	void);
extern unsigned int mt6355_upmu_get_rgs_vproc12_dig_mon(
	void);
extern unsigned int mt6355_upmu_set_rg_vproc11_tran_bst(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc12_tran_bst(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc11_cotramp_slp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc12_cotramp_slp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc11_sleep_time(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc12_sleep_time(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc11_vreftb(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc12_vreftb(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc11_csnslp_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc12_csnslp_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc11_cspslp_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc12_cspslp_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc11_fugon(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc12_fugon(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc11_flgon(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc12_flgon(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc11_preoc_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc11_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc12_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vproc11_nonaudible_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vproc11_nonaudible_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vproc12_nonaudible_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vproc12_nonaudible_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vproc_disautok(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcore_fpwm(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vgpu_fpwm(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcore_ndis_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vcore_ndis_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vgpu_ndis_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vgpu_ndis_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vcore_fcot(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vgpu_fcot(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcorevgpu_tmdl(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcorevgpu_disconfig20(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcore_tbdis(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vgpu_tbdis(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcore_vdiffoff(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vgpu_vdiffoff(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcore_rcomp0(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcore_rcomp1(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcore_ccomp0(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcore_ccomp1(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcore_ramp_slp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vgpu_rcomp0(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vgpu_rcomp1(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vgpu_ccomp0(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vgpu_ccomp1(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vgpu_ramp_slp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcore_rcs(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vgpu_rcs(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcore_rcb(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vgpu_rcb(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcore_csp_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vgpu_csp_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcore_csn_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vgpu_csn_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcore_zc_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vgpu_zc_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcore_nlim_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vgpu_nlim_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcore_rpsi1_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vgpu_rpsi1_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcore_tb_width(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vgpu_tb_width(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcore_ug_sr(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcore_lg_sr(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vgpu_ug_sr(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vgpu_lg_sr(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcore_pfm_ton(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vgpu_pfm_ton(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcore_ton_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vgpu_ton_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rgs_vcore_oc_status(
	void);
extern unsigned int mt6355_upmu_get_rgs_vgpu_oc_status(
	void);
extern unsigned int mt6355_upmu_get_rgs_vcorevgpu_trimok_status(
	void);
extern unsigned int mt6355_upmu_get_rgs_vcorevgpu_config20_status(
	void);
extern unsigned int mt6355_upmu_get_rgs_vcore_preoc_status(
	void);
extern unsigned int mt6355_upmu_get_rgs_vcore_dig_mon(
	void);
extern unsigned int mt6355_upmu_get_rgs_vgpu_dig_mon(
	void);
extern unsigned int mt6355_upmu_set_rg_vcore_tran_bst(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vgpu_tran_bst(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcore_cotramp_slp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vgpu_cotramp_slp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcore_sleep_time(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vgpu_sleep_time(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcore_vreftb(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vgpu_vreftb(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcore_csnslp_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vgpu_csnslp_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcore_cspslp_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vgpu_cspslp_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcore_fugon(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vgpu_fugon(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcore_flgon(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vgpu_flgon(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcore_preoc_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcore_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vgpu_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcore_nonaudible_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vcore_nonaudible_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vgpu_nonaudible_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vgpu_nonaudible_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vcorevgpu_disautok(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vdram1_fcot(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vdram1_rcomp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vdram1_tb_dis(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vdram1_dispg(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vdram1_fpwm(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vdram1_zc_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vdram1_nlim_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vdram1_pfm_ton(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vdram1_pwmramp_slp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vdram1_cotramp_slp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vdram1_rcs(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vdram1_csn_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vdram1_csp_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vdram1_rpsi_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vdram1_sleep_time(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vdram1_nlim_gating(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vdram1_ton_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vdram1_vdiff_off(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vdram1_vrefup(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vdram1_tb_width(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vdram1_ug_sr(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vdram1_lg_sr(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vdram1_ccomp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vdram1_ndis_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vdram1_ndis_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vdram1_tmdl(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vdram1_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vdram1_csnslp_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vdram1_cspslp_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vdram1_fugon(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vdram1_flgon(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vdram1_nonaudible_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vdram1_nonaudible_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vdram1_vdiffpfm_off(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rgs_vdram1_oc_status(
	void);
extern unsigned int mt6355_upmu_get_rgs_vdram1_enpwm_status(
	void);
extern unsigned int mt6355_upmu_set_rg_vdram1_disautok(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rgs_vdram1_trimok_status(
	void);
extern unsigned int mt6355_upmu_get_rgs_vdram1_dig_mon(
	void);
extern unsigned int mt6355_upmu_set_rg_vdram2_fcot(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vdram2_rcomp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vdram2_tb_dis(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vdram2_dispg(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vdram2_fpwm(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vdram2_zc_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vdram2_nlim_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vdram2_pfm_ton(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vdram2_pwmramp_slp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vdram2_cotramp_slp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vdram2_rcs(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vdram2_csn_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vdram2_csp_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vdram2_rpsi_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vdram2_sleep_time(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vdram2_nlim_gating(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vdram2_ton_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vdram2_vdiff_off(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vdram2_vrefup(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vdram2_tb_width(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vdram2_ug_sr(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vdram2_lg_sr(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vdram2_ccomp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vdram2_ndis_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vdram2_ndis_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vdram2_tmdl(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vdram2_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vdram2_csnslp_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vdram2_cspslp_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vdram2_fugon(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vdram2_flgon(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vdram2_nonaudible_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vdram2_nonaudible_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vdram2_vdiffpfm_off(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rgs_vdram2_oc_status(
	void);
extern unsigned int mt6355_upmu_get_rgs_vdram2_enpwm_status(
	void);
extern unsigned int mt6355_upmu_set_rg_vdram2_disautok(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rgs_vdram2_trimok_status(
	void);
extern unsigned int mt6355_upmu_get_rgs_vdram2_dig_mon(
	void);
extern unsigned int mt6355_upmu_set_rg_vmodem_modeset(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vmodem_ndis_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vmodem_ndis_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vmodem_vrf18_sstart_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vmodem_vrf18_sstart_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vmodem_auto_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vmodem_rzsel0(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vmodem_rzsel1(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vmodem_ccsel0(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vmodem_ccsel1(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vmodem_csl(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vmodem_slp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vmodem_adrc_fen(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vmodem_vc_cap_clamp_fen(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vmodem_vc_clamp_fen(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vmodem_burst(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vmodem_csr(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vmodem_zxos_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vmodem_pfmsr_eh(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vmodem_nlim_gating(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vmodem_pwmsr_eh(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vmodem_hs_vthdet(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vmodem_pg_gating(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vmodem_hs_onspeed_eh(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vmodem_nlim_trimming(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vmodem_sr_p(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vmodem_sr_n(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vmodem_pfm_rip(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vmodem_tran_bst(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vmodem_dts_enb(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vmodem_dts_enb(
	void);
extern unsigned int mt6355_upmu_set_rg_vmodem_min_off(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vmodem_1p35up_sel_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vmodem_1p35up_sel_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vmodem_dlc_auto_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vmodem_src_auto_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vmodem_ugp_sr(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vmodem_lgp_sr(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vmodem_ugp_sr_pfm(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vmodem_lgp_sr_pfm(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vmodem_ugd_vthsel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vmodem_fnlx(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vmodem_vdiff_enlowiq(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vmodem_vdiff_enlowiq(
	void);
extern unsigned int mt6355_upmu_set_rg_vmodem_pfmoc_fwupoff(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vmodem_pwmoc_fwupoff(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vmodem_cp_fwupoff(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vmodem_zx_gating(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vmodem_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vmodem_azc_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vmodem_azc_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vmodem_azc_delay(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vmodem_azc_hold_enb(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vmodem_azc_hold_enb(
	void);
extern unsigned int mt6355_upmu_get_rgs_vmodem_oc_status(
	void);
extern unsigned int mt6355_upmu_get_rgs_vmodem_dig_mon(
	void);
extern unsigned int mt6355_upmu_get_rgs_vmodem_enpwm_status(
	void);
extern unsigned int mt6355_upmu_set_rg_vmodem_iodetect_en18(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vmodem_iodetect_en18(
	void);
extern unsigned int mt6355_upmu_set_rg_vmodem_preoc_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vmodem_nonaudible_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vmodem_nonaudible_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vs1_min_off(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vs1_vrf18_sstart_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vs1_vrf18_sstart_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vs1_1p35up_sel_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vs1_1p35up_sel_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vs1_rzsel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vs1_csr(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vs1_csl(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vs1_slp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vs1_zx_os(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vs1_ndis_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vs1_ndis_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vs1_csm_n(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vs1_csm_p(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vs1_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vs1_zxos_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vs1_modeset(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vs1_pfm_rip(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vs1_tran_bst(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vs1_dts_enb(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vs1_dts_enb(
	void);
extern unsigned int mt6355_upmu_set_rg_vs1_auto_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vs1_pwm_trig(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vs1_rsv_h(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vs1_rsv_l(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vs1_nonaudible_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vs1_nonaudible_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vs1_sr_p(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vs1_sr_n(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vs1_burst(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rgs_vs1_enpwm_status(
	void);
extern unsigned int mt6355_upmu_get_rgs_vs1_oc_status(
	void);
extern unsigned int mt6355_upmu_get_rgs_vs1_dig_mon(
	void);
extern unsigned int mt6355_upmu_set_rg_vs2_min_off(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vs2_vrf18_sstart_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vs2_vrf18_sstart_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vs2_1p35up_sel_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vs2_1p35up_sel_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vs2_rzsel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vs2_csr(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vs2_csl(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vs2_slp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vs2_zx_os(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vs2_ndis_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vs2_ndis_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vs2_csm_n(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vs2_csm_p(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vs2_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vs2_zxos_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vs2_modeset(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vs2_pfm_rip(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vs2_tran_bst(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vs2_dts_enb(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vs2_dts_enb(
	void);
extern unsigned int mt6355_upmu_set_rg_vs2_auto_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vs2_pwm_trig(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vs2_rsv_h(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vs2_rsv_l(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vs2_nonaudible_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vs2_nonaudible_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vs2_sr_p(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vs2_sr_n(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vs2_burst(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rgs_vs2_enpwm_status(
	void);
extern unsigned int mt6355_upmu_get_rgs_vs2_oc_status(
	void);
extern unsigned int mt6355_upmu_get_rgs_vs2_dig_mon(
	void);
extern unsigned int mt6355_upmu_set_rg_vpa_ndis_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vpa_ndis_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vpa_modeset(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vpa_cc(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vpa_csr(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vpa_csmir(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vpa_csl(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vpa_slp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vpa_azc_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vpa_azc_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vpa_cp_fwupoff(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vpa_azc_delay(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vpa_rzsel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vpa_zxref(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vpa_nlim_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vpa_hzp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vpa_bwex_gat(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vpa_slew(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vpa_slew_nmos(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vpa_min_on(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vpa_vbat_del(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rgs_vpa_azc_vos_sel(
	void);
extern unsigned int mt6355_upmu_set_rg_vpa_min_pk(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vpa_rsv1(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vpa_rsv2(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rgs_vpa_oc_status(
	void);
extern unsigned int mt6355_upmu_get_rgs_vpa_azc_zx(
	void);
extern unsigned int mt6355_upmu_set_wdtdbg_clr(
	unsigned int val);
extern unsigned int mt6355_upmu_set_wdtdbg_con0_rsv0(
	unsigned int val);
extern unsigned int mt6355_upmu_get_vproc11_vosel_wdtdbg(
	void);
extern unsigned int mt6355_upmu_get_vproc12_vosel_wdtdbg(
	void);
extern unsigned int mt6355_upmu_get_vcore_vosel_wdtdbg(
	void);
extern unsigned int mt6355_upmu_get_vgpu_vosel_wdtdbg(
	void);
extern unsigned int mt6355_upmu_get_vdram1_vosel_wdtdbg(
	void);
extern unsigned int mt6355_upmu_get_vdram2_vosel_wdtdbg(
	void);
extern unsigned int mt6355_upmu_get_vmodem_vosel_wdtdbg(
	void);
extern unsigned int mt6355_upmu_get_vs1_vosel_wdtdbg(
	void);
extern unsigned int mt6355_upmu_get_vs2_vosel_wdtdbg(
	void);
extern unsigned int mt6355_upmu_get_vpa_vosel_wdtdbg(
	void);
extern unsigned int mt6355_upmu_get_vsram_proc_vosel_wdtdbg(
	void);
extern unsigned int mt6355_upmu_get_vsram_core_vosel_wdtdbg(
	void);
extern unsigned int mt6355_upmu_get_vsram_gpu_vosel_wdtdbg(
	void);
extern unsigned int mt6355_upmu_get_vsram_md_vosel_wdtdbg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vio28_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vio28_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vio28_lp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vio28_sw_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vio28_sw_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vio28_hw0_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vio28_hw0_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vio28_hw1_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vio28_hw1_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vio28_hw2_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vio28_hw2_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vio28_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vio28_hw0_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vio28_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vio28_hw1_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vio28_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vio28_hw2_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vio28_on_op(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vio28_lp_op(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vio28_mode(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vio28_stbtd(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vio28_stb(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vio28_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vio28_ocfb_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vio28_ocfb_en(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vio28_ocfb_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vio28_dummy_load(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vio28_dummy_load(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vio18_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vio18_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vio18_lp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vio18_sw_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vio18_sw_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vio18_hw0_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vio18_hw0_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vio18_hw1_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vio18_hw1_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vio18_hw2_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vio18_hw2_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vio18_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vio18_hw0_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vio18_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vio18_hw1_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vio18_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vio18_hw2_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vio18_on_op(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vio18_lp_op(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vio18_mode(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vio18_stbtd(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vio18_stb(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vio18_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vio18_ocfb_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vio18_ocfb_en(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vio18_ocfb_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vio18_dummy_load(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vio18_dummy_load(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vufs18_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vufs18_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vufs18_lp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vufs18_sw_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vufs18_sw_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vufs18_hw0_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vufs18_hw0_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vufs18_hw1_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vufs18_hw1_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vufs18_hw2_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vufs18_hw2_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vufs18_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vufs18_hw0_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vufs18_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vufs18_hw1_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vufs18_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vufs18_hw2_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vufs18_on_op(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vufs18_lp_op(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vufs18_mode(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vufs18_stbtd(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vufs18_stb(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vufs18_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vufs18_ocfb_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vufs18_ocfb_en(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vufs18_ocfb_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vufs18_dummy_load(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vufs18_dummy_load(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_va10_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_va10_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_va10_lp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_va10_vocal(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_va10_vosel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_va10_vosel(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_va10_sw_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_va10_sw_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_va10_hw0_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_va10_hw0_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_va10_hw1_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_va10_hw1_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_va10_hw2_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_va10_hw2_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_va10_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_va10_hw0_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_va10_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_va10_hw1_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_va10_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_va10_hw2_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_va10_on_op(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_va10_lp_op(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_va10_mode(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_va10_stbtd(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_va10_stb(
	void);
extern unsigned int mt6355_upmu_get_da_qi_va10_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_va10_sleep_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_va10_ocfb_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_va10_ocfb_en(
	void);
extern unsigned int mt6355_upmu_get_da_qi_va10_ocfb_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_va10_dummy_load(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_va10_dummy_load(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_va12_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_va12_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_va12_lp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_va12_sw_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_va12_sw_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_va12_hw0_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_va12_hw0_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_va12_hw1_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_va12_hw1_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_va12_hw2_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_va12_hw2_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_va12_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_va12_hw0_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_va12_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_va12_hw1_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_va12_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_va12_hw2_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_va12_on_op(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_va12_lp_op(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_va12_mode(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_va12_stbtd(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_va12_stb(
	void);
extern unsigned int mt6355_upmu_get_da_qi_va12_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_va12_ocfb_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_va12_ocfb_en(
	void);
extern unsigned int mt6355_upmu_get_da_qi_va12_ocfb_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_va12_dummy_load(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_va12_dummy_load(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_va18_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_va18_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_va18_lp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_va18_sw_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_va18_sw_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_va18_hw0_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_va18_hw0_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_va18_hw1_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_va18_hw1_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_va18_hw2_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_va18_hw2_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_va18_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_va18_hw0_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_va18_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_va18_hw1_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_va18_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_va18_hw2_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_va18_on_op(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_va18_lp_op(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_va18_mode(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_va18_stbtd(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_va18_stb(
	void);
extern unsigned int mt6355_upmu_get_da_qi_va18_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_va18_auxadc_pwdb_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_va18_auxadc_pwdb_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_va18_ocfb_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_va18_ocfb_en(
	void);
extern unsigned int mt6355_upmu_get_da_qi_va18_ocfb_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_va18_dummy_load(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_va18_dummy_load(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vusb33_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vusb33_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vusb33_lp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vusb33_sw_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vusb33_sw_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vusb33_hw0_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vusb33_hw0_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vusb33_hw1_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vusb33_hw1_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vusb33_hw2_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vusb33_hw2_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vusb33_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vusb33_hw0_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vusb33_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vusb33_hw1_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vusb33_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vusb33_hw2_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vusb33_on_op(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vusb33_lp_op(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vusb33_1_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vusb33_1_en(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vusb33_mode(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vusb33_stbtd(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vusb33_stb(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vusb33_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vusb33_ocfb_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vusb33_ocfb_en(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vusb33_ocfb_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vusb33_dummy_load(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vusb33_dummy_load(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vemc_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vemc_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vemc_lp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vemc_sw_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vemc_sw_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vemc_hw0_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vemc_hw0_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vemc_hw1_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vemc_hw1_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vemc_hw2_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vemc_hw2_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vemc_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vemc_hw0_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vemc_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vemc_hw1_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vemc_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vemc_hw2_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vemc_on_op(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vemc_lp_op(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vemc_mode(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vemc_stbtd(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vemc_stb(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vemc_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vemc_ocfb_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vemc_ocfb_en(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vemc_ocfb_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vemc_dummy_load(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vemc_dummy_load(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vxo22_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vxo22_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vxo22_lp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vxo22_sw_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vxo22_sw_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vxo22_hw0_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vxo22_hw0_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vxo22_hw1_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vxo22_hw1_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vxo22_hw2_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vxo22_hw2_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vxo22_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vxo22_hw0_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vxo22_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vxo22_hw1_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vxo22_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vxo22_hw2_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vxo22_on_op(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vxo22_lp_op(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vxo22_mode(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vxo22_stbtd(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vxo22_stb(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vxo22_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vxo22_ocfb_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vxo22_ocfb_en(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vxo22_ocfb_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vxo22_dummy_load(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vxo22_dummy_load(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vxo18_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vxo18_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vxo18_lp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vxo18_sw_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vxo18_sw_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vxo18_hw0_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vxo18_hw0_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vxo18_hw1_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vxo18_hw1_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vxo18_hw2_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vxo18_hw2_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vxo18_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vxo18_hw0_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vxo18_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vxo18_hw1_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vxo18_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vxo18_hw2_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vxo18_on_op(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vxo18_lp_op(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vxo18_mode(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vxo18_stbtd(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vxo18_stb(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vxo18_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vxo18_ocfb_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vxo18_ocfb_en(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vxo18_ocfb_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vxo18_dummy_load(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vxo18_dummy_load(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsim1_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsim1_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsim1_lp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vsim1_sw_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsim1_sw_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsim1_hw0_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsim1_hw0_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsim1_hw1_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsim1_hw1_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsim1_hw2_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsim1_hw2_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsim1_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsim1_hw0_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsim1_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsim1_hw1_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsim1_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsim1_hw2_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsim1_on_op(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vsim1_lp_op(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vsim1_mode(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsim1_stbtd(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vsim1_stb(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vsim1_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsim1_ocfb_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsim1_ocfb_en(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vsim1_ocfb_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsim1_dummy_load(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vsim1_dummy_load(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsim2_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsim2_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsim2_lp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vsim2_sw_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsim2_sw_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsim2_hw0_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsim2_hw0_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsim2_hw1_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsim2_hw1_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsim2_hw2_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsim2_hw2_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsim2_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsim2_hw0_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsim2_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsim2_hw1_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsim2_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsim2_hw2_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsim2_on_op(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vsim2_lp_op(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vsim2_mode(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsim2_stbtd(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vsim2_stb(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vsim2_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsim2_ocfb_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsim2_ocfb_en(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vsim2_ocfb_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsim2_dummy_load(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vsim2_dummy_load(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcamd1_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcamd1_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcamd1_lp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vcamd1_sw_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcamd1_sw_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcamd1_hw0_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcamd1_hw0_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcamd1_hw1_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcamd1_hw1_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcamd1_hw2_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcamd1_hw2_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcamd1_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcamd1_hw0_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcamd1_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcamd1_hw1_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcamd1_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcamd1_hw2_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcamd1_on_op(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vcamd1_lp_op(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vcamd1_mode(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcamd1_stbtd(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vcamd1_stb(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vcamd1_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcamd1_ocfb_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcamd1_ocfb_en(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vcamd1_ocfb_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcamd1_dummy_load(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vcamd1_dummy_load(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcamd2_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcamd2_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcamd2_lp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vcamd2_sw_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcamd2_sw_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcamd2_hw0_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcamd2_hw0_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcamd2_hw1_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcamd2_hw1_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcamd2_hw2_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcamd2_hw2_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcamd2_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcamd2_hw0_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcamd2_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcamd2_hw1_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcamd2_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcamd2_hw2_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcamd2_on_op(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vcamd2_lp_op(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vcamd2_mode(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcamd2_stbtd(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vcamd2_stb(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vcamd2_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcamd2_ocfb_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcamd2_ocfb_en(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vcamd2_ocfb_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcamd2_dummy_load(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vcamd2_dummy_load(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcamio_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcamio_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcamio_lp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vcamio_sw_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcamio_sw_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcamio_hw0_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcamio_hw0_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcamio_hw1_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcamio_hw1_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcamio_hw2_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcamio_hw2_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcamio_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcamio_hw0_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcamio_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcamio_hw1_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcamio_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcamio_hw2_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcamio_on_op(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vcamio_lp_op(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vcamio_mode(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcamio_stbtd(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vcamio_stb(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vcamio_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcamio_ocfb_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcamio_ocfb_en(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vcamio_ocfb_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcamio_dummy_load(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vcamio_dummy_load(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vmipi_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vmipi_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vmipi_lp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vmipi_sw_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vmipi_sw_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vmipi_hw0_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vmipi_hw0_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vmipi_hw1_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vmipi_hw1_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vmipi_hw2_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vmipi_hw2_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vmipi_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vmipi_hw0_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vmipi_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vmipi_hw1_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vmipi_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vmipi_hw2_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vmipi_on_op(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vmipi_lp_op(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vmipi_mode(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vmipi_stbtd(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vmipi_stb(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vmipi_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vmipi_ocfb_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vmipi_ocfb_en(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vmipi_ocfb_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vmipi_dummy_load(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vmipi_dummy_load(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vgp_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vgp_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vgp_lp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vgp_sw_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vgp_sw_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vgp_hw0_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vgp_hw0_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vgp_hw1_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vgp_hw1_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vgp_hw2_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vgp_hw2_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vgp_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vgp_hw0_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vgp_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vgp_hw1_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vgp_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vgp_hw2_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vgp_on_op(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vgp_lp_op(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vgp_mode(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vgp_stbtd(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vgp_stb(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vgp_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vgp_ocfb_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vgp_ocfb_en(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vgp_ocfb_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vgp_dummy_load(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vgp_dummy_load(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcn33_bt_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcn33_bt_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcn33_lp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vcn33_sw_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcn33_sw_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcn33_hw0_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcn33_hw0_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcn33_hw1_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcn33_hw1_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcn33_hw2_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcn33_hw2_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcn33_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcn33_hw0_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcn33_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcn33_hw1_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcn33_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcn33_hw2_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcn33_on_op(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vcn33_lp_op(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vcn33_wifi_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcn33_wifi_en(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vcn33_mode(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcn33_stbtd(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vcn33_stb(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vcn33_en(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vcn33_bt_en(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vcn33_wifi_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcn33_ocfb_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcn33_ocfb_en(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vcn33_ocfb_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcn33_dummy_load(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vcn33_dummy_load(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcn18_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcn18_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcn18_lp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vcn18_sw_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcn18_sw_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcn18_hw0_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcn18_hw0_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcn18_hw1_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcn18_hw1_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcn18_hw2_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcn18_hw2_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcn18_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcn18_hw0_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcn18_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcn18_hw1_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcn18_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcn18_hw2_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcn18_on_op(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vcn18_lp_op(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vcn18_mode(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcn18_stbtd(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vcn18_stb(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vcn18_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcn18_ocfb_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcn18_ocfb_en(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vcn18_ocfb_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcn18_dummy_load(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vcn18_dummy_load(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcn28_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcn28_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcn28_lp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vcn28_sw_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcn28_sw_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcn28_hw0_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcn28_hw0_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcn28_hw1_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcn28_hw1_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcn28_hw2_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcn28_hw2_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcn28_hw3_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcn28_hw3_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcn28_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcn28_hw0_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcn28_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcn28_hw1_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcn28_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcn28_hw2_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcn28_hw3_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcn28_hw3_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcn28_on_op(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vcn28_lp_op(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vcn28_mode(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcn28_stbtd(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vcn28_stb(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vcn28_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcn28_ocfb_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcn28_ocfb_en(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vcn28_ocfb_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcn28_dummy_load(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vcn28_dummy_load(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vbif28_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vbif28_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vbif28_lp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vbif28_sw_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vbif28_sw_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vbif28_hw0_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vbif28_hw0_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vbif28_hw1_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vbif28_hw1_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vbif28_hw2_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vbif28_hw2_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vbif28_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vbif28_hw0_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vbif28_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vbif28_hw1_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vbif28_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vbif28_hw2_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vbif28_on_op(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vbif28_lp_op(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vbif28_mode(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vbif28_stbtd(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vbif28_stb(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vbif28_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vbif28_ocfb_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vbif28_ocfb_en(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vbif28_ocfb_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vbif28_dummy_load(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vbif28_dummy_load(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vtcxo24_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vtcxo24_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vtcxo24_lp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vtcxo24_sw_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vtcxo24_sw_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vtcxo24_hw0_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vtcxo24_hw0_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vtcxo24_hw1_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vtcxo24_hw1_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vtcxo24_hw2_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vtcxo24_hw2_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vtcxo24_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vtcxo24_hw0_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vtcxo24_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vtcxo24_hw1_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vtcxo24_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vtcxo24_hw2_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vtcxo24_on_op(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vtcxo24_lp_op(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vtcxo24_mode(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vtcxo24_stbtd(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vtcxo24_stb(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vtcxo24_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vtcxo24_ocfb_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vtcxo24_ocfb_en(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vtcxo24_ocfb_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vtcxo24_dummy_load(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vtcxo24_dummy_load(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vldo28_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vldo28_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vldo28_lp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vldo28_sw_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vldo28_sw_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vldo28_hw0_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vldo28_hw0_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vldo28_hw1_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vldo28_hw1_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vldo28_hw2_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vldo28_hw2_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vldo28_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vldo28_hw0_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vldo28_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vldo28_hw1_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vldo28_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vldo28_hw2_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vldo28_on_op(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vldo28_lp_op(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vldo28_tp_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vldo28_tp_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vldo28_1_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vldo28_1_en(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vldo28_mode(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vldo28_stbtd(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vldo28_stb(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vldo28_en(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vldo28_tp_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vldo28_ocfb_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vldo28_ocfb_en(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vldo28_ocfb_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vldo28_dummy_load(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vldo28_dummy_load(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vgp2_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vgp2_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vgp2_lp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vgp2_sw_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vgp2_sw_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vgp2_hw0_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vgp2_hw0_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vgp2_hw1_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vgp2_hw1_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vgp2_hw2_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vgp2_hw2_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vgp2_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vgp2_hw0_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vgp2_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vgp2_hw1_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vgp2_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vgp2_hw2_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vgp2_on_op(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vgp2_lp_op(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vgp2_mode(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vgp2_stbtd(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vgp2_stb(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vgp2_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vgp2_ther_sdn_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vgp2_ther_sdn_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vgp2_ocfb_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vgp2_ocfb_en(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vgp2_ocfb_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vgp2_dummy_load(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vgp2_dummy_load(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vfe28_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vfe28_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vfe28_lp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vfe28_sw_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vfe28_sw_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vfe28_hw0_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vfe28_hw0_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vfe28_hw1_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vfe28_hw1_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vfe28_hw2_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vfe28_hw2_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vfe28_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vfe28_hw0_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vfe28_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vfe28_hw1_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vfe28_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vfe28_hw2_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vfe28_on_op(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vfe28_lp_op(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vfe28_mode(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vfe28_stbtd(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vfe28_stb(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vfe28_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vfe28_ocfb_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vfe28_ocfb_en(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vfe28_ocfb_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vfe28_dummy_load(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vfe28_dummy_load(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vmch_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vmch_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vmch_lp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vmch_sw_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vmch_sw_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vmch_hw0_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vmch_hw0_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vmch_hw1_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vmch_hw1_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vmch_hw2_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vmch_hw2_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vmch_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vmch_hw0_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vmch_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vmch_hw1_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vmch_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vmch_hw2_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vmch_on_op(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vmch_lp_op(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vmch_mode(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vmch_stbtd(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vmch_stb(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vmch_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vmch_ocfb_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vmch_ocfb_en(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vmch_ocfb_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vmch_dummy_load(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vmch_dummy_load(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vmc_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vmc_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vmc_lp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vmc_sw_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vmc_sw_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vmc_hw0_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vmc_hw0_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vmc_hw1_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vmc_hw1_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vmc_hw2_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vmc_hw2_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vmc_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vmc_hw0_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vmc_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vmc_hw1_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vmc_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vmc_hw2_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vmc_on_op(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vmc_lp_op(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vmc_mode(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vmc_stbtd(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vmc_stb(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vmc_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vmc_ocfb_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vmc_ocfb_en(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vmc_ocfb_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vmc_dummy_load(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vmc_dummy_load(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vrf18_1_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vrf18_1_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vrf18_1_lp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vrf18_1_sw_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vrf18_1_sw_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vrf18_1_hw0_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vrf18_1_hw0_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vrf18_1_hw1_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vrf18_1_hw1_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vrf18_1_hw2_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vrf18_1_hw2_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vrf18_1_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vrf18_1_hw0_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vrf18_1_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vrf18_1_hw1_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vrf18_1_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vrf18_1_hw2_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vrf18_1_on_op(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vrf18_1_lp_op(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vrf18_1_mode(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vrf18_1_stbtd(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vrf18_1_stb(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vrf18_1_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vrf18_1_ocfb_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vrf18_1_ocfb_en(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vrf18_1_ocfb_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vrf18_1_dummy_load(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vrf18_1_dummy_load(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vrf18_2_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vrf18_2_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vrf18_2_lp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vrf18_2_sw_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vrf18_2_sw_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vrf18_2_hw0_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vrf18_2_hw0_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vrf18_2_hw1_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vrf18_2_hw1_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vrf18_2_hw2_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vrf18_2_hw2_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vrf18_2_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vrf18_2_hw0_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vrf18_2_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vrf18_2_hw1_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vrf18_2_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vrf18_2_hw2_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vrf18_2_on_op(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vrf18_2_lp_op(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vrf18_2_mode(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vrf18_2_stbtd(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vrf18_2_stb(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vrf18_2_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vrf18_2_ocfb_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vrf18_2_ocfb_en(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vrf18_2_ocfb_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vrf18_2_dummy_load(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vrf18_2_dummy_load(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vrf12_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vrf12_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vrf12_lp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vrf12_sw_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vrf12_sw_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vrf12_hw0_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vrf12_hw0_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vrf12_hw1_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vrf12_hw1_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vrf12_hw2_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vrf12_hw2_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vrf12_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vrf12_hw0_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vrf12_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vrf12_hw1_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vrf12_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vrf12_hw2_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vrf12_on_op(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vrf12_lp_op(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vrf12_mode(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vrf12_stbtd(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vrf12_stb(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vrf12_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vrf12_ocfb_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vrf12_ocfb_en(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vrf12_ocfb_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vrf12_dummy_load(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vrf12_dummy_load(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcama1_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcama1_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcama1_lp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vcama1_sw_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcama1_sw_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcama1_hw0_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcama1_hw0_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcama1_hw1_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcama1_hw1_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcama1_hw2_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcama1_hw2_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcama1_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcama1_hw0_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcama1_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcama1_hw1_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcama1_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcama1_hw2_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcama1_on_op(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vcama1_lp_op(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vcama1_mode(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcama1_stbtd(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vcama1_stb(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vcama1_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcama1_ocfb_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcama1_ocfb_en(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vcama1_ocfb_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcama1_dummy_load(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vcama1_dummy_load(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcama2_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcama2_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcama2_lp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vcama2_sw_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcama2_sw_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcama2_hw0_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcama2_hw0_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcama2_hw1_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcama2_hw1_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcama2_hw2_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcama2_hw2_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcama2_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcama2_hw0_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcama2_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcama2_hw1_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcama2_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcama2_hw2_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcama2_on_op(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vcama2_lp_op(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vcama2_mode(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcama2_stbtd(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vcama2_stb(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vcama2_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcama2_ocfb_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcama2_ocfb_en(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vcama2_ocfb_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcama2_dummy_load(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vcama2_dummy_load(
	void);
extern unsigned int mt6355_upmu_set_ldo_degtd_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vrtc_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vrtc_en(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vrtc_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_rsv1(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_rsv0(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_rsv2(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_proc_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsram_proc_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_proc_lp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_proc_vosel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsram_proc_vosel(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_proc_vosel_sleep(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsram_proc_vosel_sleep(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_proc_sfchg_frate(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_proc_sfchg_fen(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_proc_sfchg_rrate(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_proc_sfchg_ren(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_proc_dvs_trans_td(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_proc_dvs_trans_ctrl(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_proc_dvs_trans_once(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_proc_sw_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsram_proc_sw_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_proc_hw0_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsram_proc_hw0_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_proc_hw1_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsram_proc_hw1_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_proc_hw2_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsram_proc_hw2_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_proc_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsram_proc_hw0_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_proc_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsram_proc_hw1_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_proc_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsram_proc_hw2_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_proc_on_op(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_proc_lp_op(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vsram_proc_mode(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_proc_stbtd(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_proc_ocfb_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsram_proc_ocfb_en(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vsram_proc_ocfb_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_proc_dummy_load(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vsram_proc_dummy_load(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vsram_proc_vosel_gray(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vsram_proc_vosel(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vsram_proc_en(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vsram_proc_stb(
	void);
extern unsigned int mt6355_upmu_get_da_ni_vsram_proc_vsleep_sel(
	void);
extern unsigned int mt6355_upmu_get_da_ni_vsram_proc_r2r_pdn(
	void);
extern unsigned int mt6355_upmu_get_da_ni_vsram_proc_track_ndis_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_core_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsram_core_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_core_lp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_core_vosel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsram_core_vosel(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_core_vosel_sleep(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsram_core_vosel_sleep(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_core_sfchg_frate(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_core_sfchg_fen(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_core_sfchg_rrate(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_core_sfchg_ren(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_core_dvs_trans_td(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_core_dvs_trans_ctrl(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_core_dvs_trans_once(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_core_sw_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsram_core_sw_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_core_hw0_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsram_core_hw0_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_core_hw1_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsram_core_hw1_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_core_hw2_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsram_core_hw2_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_core_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsram_core_hw0_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_core_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsram_core_hw1_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_core_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsram_core_hw2_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_core_on_op(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_core_lp_op(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vsram_core_mode(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_core_stbtd(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_core_ocfb_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsram_core_ocfb_en(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vsram_core_ocfb_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_core_dummy_load(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vsram_core_dummy_load(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vsram_core_vosel_gray(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vsram_core_vosel(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vsram_core_en(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vsram_core_stb(
	void);
extern unsigned int mt6355_upmu_get_da_ni_vsram_core_vsleep_sel(
	void);
extern unsigned int mt6355_upmu_get_da_ni_vsram_core_r2r_pdn(
	void);
extern unsigned int mt6355_upmu_get_da_ni_vsram_core_track_ndis_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_gpu_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsram_gpu_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_gpu_lp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_gpu_vosel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsram_gpu_vosel(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_gpu_vosel_sleep(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsram_gpu_vosel_sleep(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_gpu_sfchg_frate(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_gpu_sfchg_fen(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_gpu_sfchg_rrate(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_gpu_sfchg_ren(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_gpu_dvs_trans_td(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_gpu_dvs_trans_ctrl(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_gpu_dvs_trans_once(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_gpu_sw_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsram_gpu_sw_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_gpu_hw0_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsram_gpu_hw0_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_gpu_hw1_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsram_gpu_hw1_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_gpu_hw2_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsram_gpu_hw2_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_gpu_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsram_gpu_hw0_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_gpu_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsram_gpu_hw1_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_gpu_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsram_gpu_hw2_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_gpu_on_op(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_gpu_lp_op(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vsram_gpu_mode(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_gpu_stbtd(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_gpu_ocfb_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsram_gpu_ocfb_en(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vsram_gpu_ocfb_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_gpu_dummy_load(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vsram_gpu_dummy_load(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vsram_gpu_vosel_gray(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vsram_gpu_vosel(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vsram_gpu_en(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vsram_gpu_stb(
	void);
extern unsigned int mt6355_upmu_get_da_ni_vsram_gpu_vsleep_sel(
	void);
extern unsigned int mt6355_upmu_get_da_ni_vsram_gpu_r2r_pdn(
	void);
extern unsigned int mt6355_upmu_get_da_ni_vsram_gpu_track_ndis_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_md_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsram_md_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_md_lp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_md_vosel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsram_md_vosel(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_md_vosel_sleep(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsram_md_vosel_sleep(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_md_sfchg_frate(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_md_sfchg_fen(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_md_sfchg_rrate(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_md_sfchg_ren(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_md_dvs_trans_td(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_md_dvs_trans_ctrl(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_md_dvs_trans_once(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_md_sw_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsram_md_sw_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_md_hw0_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsram_md_hw0_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_md_hw1_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsram_md_hw1_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_md_hw2_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsram_md_hw2_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_md_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsram_md_hw0_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_md_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsram_md_hw1_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_md_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsram_md_hw2_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_md_on_op(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_md_lp_op(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vsram_md_mode(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_md_stbtd(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_md_ocfb_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsram_md_ocfb_en(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vsram_md_ocfb_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_md_dummy_load(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_vsram_md_dummy_load(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vsram_md_vosel_gray(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vsram_md_vosel(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vsram_md_en(
	void);
extern unsigned int mt6355_upmu_get_da_qi_vsram_md_stb(
	void);
extern unsigned int mt6355_upmu_get_da_ni_vsram_md_vsleep_sel(
	void);
extern unsigned int mt6355_upmu_get_da_ni_vsram_md_r2r_pdn(
	void);
extern unsigned int mt6355_upmu_get_da_ni_vsram_md_track_ndis_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_core_track_sleep_ctrl(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_core_track_on_ctrl(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_core_track_vbuck_on_ctrl(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_core_vosel_delta(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsram_core_vosel_delta(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_core_vosel_offset(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsram_core_vosel_offset(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_core_vosel_on_lb(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsram_core_vosel_on_lb(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_core_vosel_on_hb(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsram_core_vosel_on_hb(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_core_vosel_sleep_lb(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsram_core_vosel_sleep_lb(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_dcm_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vio28_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vio28_ck_sw_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vio28_ck_sw_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vio28_osc_sel_dis(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vio18_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vio18_ck_sw_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vio18_ck_sw_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vio18_osc_sel_dis(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vufs18_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vufs18_ck_sw_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vufs18_ck_sw_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vufs18_osc_sel_dis(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_va10_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_va10_ck_sw_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_va10_ck_sw_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_va10_osc_sel_dis(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_va12_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_va12_ck_sw_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_va12_ck_sw_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_va12_osc_sel_dis(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_proc_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_proc_ck_sw_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsram_proc_ck_sw_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_proc_osc_sel_dis(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_core_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_core_ck_sw_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsram_core_ck_sw_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_core_osc_sel_dis(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_gpu_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_gpu_ck_sw_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsram_gpu_ck_sw_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_gpu_osc_sel_dis(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_md_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_md_ck_sw_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsram_md_ck_sw_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_md_osc_sel_dis(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_va18_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_va18_ck_sw_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_va18_ck_sw_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_va18_osc_sel_dis(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vusb33_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vusb33_ck_sw_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vusb33_ck_sw_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vusb33_osc_sel_dis(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vemc_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vemc_ck_sw_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vemc_ck_sw_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vemc_osc_sel_dis(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vxo22_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vxo22_ck_sw_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vxo22_ck_sw_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vxo22_osc_sel_dis(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vxo18_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vxo18_ck_sw_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vxo18_ck_sw_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vxo18_osc_sel_dis(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vsim1_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vsim1_ck_sw_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsim1_ck_sw_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsim1_osc_sel_dis(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vsim2_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vsim2_ck_sw_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsim2_ck_sw_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsim2_osc_sel_dis(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vcamd1_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vcamd1_ck_sw_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcamd1_ck_sw_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcamd1_osc_sel_dis(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vcamd2_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vcamd2_ck_sw_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcamd2_ck_sw_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcamd2_osc_sel_dis(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vcamio_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vcamio_ck_sw_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcamio_ck_sw_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcamio_osc_sel_dis(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vmipi_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vmipi_ck_sw_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vmipi_ck_sw_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vmipi_osc_sel_dis(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vgp_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vgp_ck_sw_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vgp_ck_sw_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vgp_osc_sel_dis(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vcn33_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vcn33_ck_sw_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcn33_ck_sw_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcn33_osc_sel_dis(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vcn18_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vcn18_ck_sw_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcn18_ck_sw_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcn18_osc_sel_dis(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vcn28_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vcn28_ck_sw_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcn28_ck_sw_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcn28_osc_sel_dis(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vgp2_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vgp2_ck_sw_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vgp2_ck_sw_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vgp2_osc_sel_dis(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vbif28_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vbif28_ck_sw_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vbif28_ck_sw_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vbif28_osc_sel_dis(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vfe28_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vfe28_ck_sw_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vfe28_ck_sw_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vfe28_osc_sel_dis(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vmch_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vmch_ck_sw_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vmch_ck_sw_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vmch_osc_sel_dis(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vmc_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vmc_ck_sw_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vmc_ck_sw_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vmc_osc_sel_dis(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vrf18_1_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vrf18_1_ck_sw_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vrf18_1_ck_sw_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vrf18_1_osc_sel_dis(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vrf18_2_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vrf18_2_ck_sw_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vrf18_2_ck_sw_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vrf18_2_osc_sel_dis(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vtcxo24_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vtcxo24_ck_sw_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vtcxo24_ck_sw_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vtcxo24_osc_sel_dis(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vldo28_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vldo28_ck_sw_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vldo28_ck_sw_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vldo28_osc_sel_dis(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vrf12_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vrf12_ck_sw_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vrf12_ck_sw_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vrf12_osc_sel_dis(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vcama1_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vcama1_ck_sw_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcama1_ck_sw_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcama1_osc_sel_dis(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vcama2_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vcama2_ck_sw_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcama2_ck_sw_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcama2_osc_sel_dis(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_proc_sp_sw_vosel_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsram_proc_sp_sw_vosel_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_proc_sp_sw_vosel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsram_proc_sp_sw_vosel(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_gpu_sp_sw_vosel_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsram_gpu_sp_sw_vosel_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_gpu_sp_sw_vosel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsram_gpu_sp_sw_vosel(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_md_sp_sw_vosel_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsram_md_sp_sw_vosel_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_md_sp_sw_vosel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsram_md_sp_sw_vosel(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_core_sp_sw_vosel_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsram_core_sp_sw_vosel_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_core_sp_sw_vosel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsram_core_sp_sw_vosel(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_core_sshub_on(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_core_sshub_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_core_sshub_vosel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vsram_core_sshub_vosel(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_lp_prot_disable(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_dummy_load_gated_dis(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_proc_r2r_pdn_dis(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_core_r2r_pdn_dis(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_gpu_r2r_pdn_dis(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ldo_vsram_md_r2r_pdn_dis(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vfe28_vocal(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vfe28_vosel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vfe28_vosel(
	void);
extern unsigned int mt6355_upmu_set_rg_vfe28_votrim(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rgs_vfe28_cal_indi(
	void);
extern unsigned int mt6355_upmu_set_rg_vfe28_oc_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vfe28_ndis_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vfe28_ndis_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vfe28_stb_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vfe28_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vfe28_vos_cal_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vfe28_vos_cal_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vtcxo24_vocal(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vtcxo24_vosel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vtcxo24_vosel(
	void);
extern unsigned int mt6355_upmu_set_rg_vtcxo24_votrim(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rgs_vtcxo24_cal_indi(
	void);
extern unsigned int mt6355_upmu_set_rg_vtcxo24_oc_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vtcxo24_ndis_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vtcxo24_ndis_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vtcxo24_stb_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vtcxo24_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vtcxo24_vos_cal_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vtcxo24_vos_cal_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vxo22_vocal(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vxo22_vosel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vxo22_vosel(
	void);
extern unsigned int mt6355_upmu_set_rg_vxo22_votrim(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rgs_vxo22_cal_indi(
	void);
extern unsigned int mt6355_upmu_set_rg_vxo22_oc_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vxo22_ndis_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vxo22_ndis_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vxo22_stb_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vxo22_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vxo22_vos_cal_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vxo22_vos_cal_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vcn28_vocal(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcn28_vosel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vcn28_vosel(
	void);
extern unsigned int mt6355_upmu_set_rg_vcn28_votrim(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rgs_vcn28_cal_indi(
	void);
extern unsigned int mt6355_upmu_set_rg_vcn28_oc_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcn28_ndis_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vcn28_ndis_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vcn28_stb_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcn28_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcn28_vos_cal_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vcn28_vos_cal_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vcama1_vocal(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcama1_vosel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vcama1_vosel(
	void);
extern unsigned int mt6355_upmu_set_rg_vcama1_votrim(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rgs_vcama1_cal_indi(
	void);
extern unsigned int mt6355_upmu_set_rg_vcama1_oc_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcama1_ndis_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vcama1_ndis_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vcama1_stb_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcama1_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcama1_vos_cal_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vcama1_vos_cal_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vcama2_vocal(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcama2_vosel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vcama2_vosel(
	void);
extern unsigned int mt6355_upmu_set_rg_vcama2_votrim(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rgs_vcama2_cal_indi(
	void);
extern unsigned int mt6355_upmu_set_rg_vcama2_oc_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcama2_ndis_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vcama2_ndis_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vcama2_stb_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcama2_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcama2_vos_cal_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vcama2_vos_cal_en(
	void);
extern unsigned int mt6355_upmu_set_rg_va18_vocal(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_va18_vosel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_va18_vosel(
	void);
extern unsigned int mt6355_upmu_set_rg_va18_votrim(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rgs_va18_cal_indi(
	void);
extern unsigned int mt6355_upmu_set_rg_va18_oc_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_va18_ndis_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_va18_ndis_en(
	void);
extern unsigned int mt6355_upmu_set_rg_va18_stb_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_va18_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_va18_vos_cal_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_va18_vos_cal_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vusb33_vocal(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vusb33_vosel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vusb33_vosel(
	void);
extern unsigned int mt6355_upmu_set_rg_vusb33_votrim(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rgs_vusb33_cal_indi(
	void);
extern unsigned int mt6355_upmu_set_rg_vusb33_oc_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vusb33_ndis_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vusb33_ndis_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vusb33_stb_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vusb33_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vusb33_vos_cal_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vusb33_vos_cal_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vbif28_vocal(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vbif28_vosel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vbif28_vosel(
	void);
extern unsigned int mt6355_upmu_set_rg_vbif28_votrim(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rgs_vbif28_cal_indi(
	void);
extern unsigned int mt6355_upmu_set_rg_vbif28_oc_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vbif28_ndis_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vbif28_ndis_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vbif28_stb_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vbif28_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vbif28_vos_cal_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vbif28_vos_cal_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vgp2_vocal(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vgp2_vosel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vgp2_vosel(
	void);
extern unsigned int mt6355_upmu_set_rg_vgp2_votrim(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rgs_vgp2_cal_indi(
	void);
extern unsigned int mt6355_upmu_set_rg_vgp2_oc_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vgp2_ndis_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vgp2_ndis_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vgp2_stb_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vgp2_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vgp2_vos_cal_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vgp2_vos_cal_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vcn33_vocal(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcn33_vosel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vcn33_vosel(
	void);
extern unsigned int mt6355_upmu_set_rg_vcn33_votrim(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rgs_vcn33_cal_indi(
	void);
extern unsigned int mt6355_upmu_set_rg_vcn33_oc_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcn33_ndis_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vcn33_ndis_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vcn33_stb_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcn33_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcn33_vos_cal_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vcn33_vos_cal_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vsim1_vocal(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vsim1_vosel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vsim1_vosel(
	void);
extern unsigned int mt6355_upmu_set_rg_vsim1_votrim(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rgs_vsim1_cal_indi(
	void);
extern unsigned int mt6355_upmu_set_rg_vsim1_oc_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vsim1_ndis_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vsim1_ndis_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vsim1_stb_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vsim1_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vsim1_vos_cal_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vsim1_vos_cal_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vsim2_vocal(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vsim2_vosel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vsim2_vosel(
	void);
extern unsigned int mt6355_upmu_set_rg_vsim2_votrim(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rgs_vsim2_cal_indi(
	void);
extern unsigned int mt6355_upmu_set_rg_vsim2_oc_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vsim2_ndis_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vsim2_ndis_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vsim2_stb_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vsim2_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vsim2_vos_cal_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vsim2_vos_cal_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vldo28_vocal(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vldo28_vosel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vldo28_vosel(
	void);
extern unsigned int mt6355_upmu_set_rg_vldo28_tp_vosel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vldo28_tp_vosel(
	void);
extern unsigned int mt6355_upmu_set_rg_vldo28_votrim(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rgs_vldo28_cal_indi(
	void);
extern unsigned int mt6355_upmu_set_rg_vldo28_oc_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vldo28_ndis_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vldo28_ndis_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vldo28_stb_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vldo28_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vldo28_vos_cal_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vldo28_vos_cal_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vio28_vocal(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vio28_vosel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vio28_vosel(
	void);
extern unsigned int mt6355_upmu_set_rg_vio28_votrim(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rgs_vio28_cal_indi(
	void);
extern unsigned int mt6355_upmu_set_rg_vio28_oc_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vio28_ndis_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vio28_ndis_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vio28_stb_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vio28_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vio28_vos_cal_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vio28_vos_cal_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vmc_vocal(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vmc_vosel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vmc_vosel(
	void);
extern unsigned int mt6355_upmu_set_rg_vmc_votrim(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rgs_vmc_cal_indi(
	void);
extern unsigned int mt6355_upmu_set_rg_vmc_oc_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vmc_ndis_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vmc_ndis_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vmc_stb_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vmc_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vmc_vos_cal_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vmc_vos_cal_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vmch_vocal(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vmch_vosel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vmch_vosel(
	void);
extern unsigned int mt6355_upmu_set_rg_vmch_votrim(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rgs_vmch_cal_indi(
	void);
extern unsigned int mt6355_upmu_set_rg_vmch_oc_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vmch_ndis_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vmch_ndis_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vmch_stb_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vmch_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vmch_vos_cal_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vmch_vos_cal_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vemc_vocal(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vemc_vosel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vemc_vosel(
	void);
extern unsigned int mt6355_upmu_set_rg_vemc_votrim(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rgs_vemc_cal_indi(
	void);
extern unsigned int mt6355_upmu_set_rg_vemc_oc_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vemc_ndis_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vemc_ndis_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vemc_stb_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vemc_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vemc_vos_cal_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vemc_vos_cal_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vgp_vocal(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vgp_vosel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vgp_vosel(
	void);
extern unsigned int mt6355_upmu_set_rg_vgp_votrim(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rgs_vgp_cal_indi(
	void);
extern unsigned int mt6355_upmu_set_rg_vgp_oc_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vgp_ndis_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vgp_ndis_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vgp_stb_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vgp_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vgp_vos_cal_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vgp_vos_cal_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vrf18_1_vocal(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vrf18_1_vosel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vrf18_1_vosel(
	void);
extern unsigned int mt6355_upmu_set_rg_vrf18_1_votrim(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rgs_vrf18_1_cal_indi(
	void);
extern unsigned int mt6355_upmu_set_rg_vrf18_1_oc_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vrf18_1_ndis_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vrf18_1_ndis_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vrf18_1_stb_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vrf18_1_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vrf18_1_vos_cal_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vrf18_1_vos_cal_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vrf18_2_vocal(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vrf18_2_vosel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vrf18_2_vosel(
	void);
extern unsigned int mt6355_upmu_set_rg_vrf18_2_votrim(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rgs_vrf18_2_cal_indi(
	void);
extern unsigned int mt6355_upmu_set_rg_vrf18_2_oc_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vrf18_2_ndis_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vrf18_2_ndis_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vrf18_2_stb_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vrf18_2_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vrf18_2_vos_cal_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vrf18_2_vos_cal_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vcn18_vocal(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcn18_vosel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vcn18_vosel(
	void);
extern unsigned int mt6355_upmu_set_rg_vcn18_votrim(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rgs_vcn18_cal_indi(
	void);
extern unsigned int mt6355_upmu_set_rg_vcn18_oc_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcn18_ndis_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vcn18_ndis_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vcn18_stb_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcn18_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcn18_vos_cal_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vcn18_vos_cal_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vcamio_vocal(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcamio_vosel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vcamio_vosel(
	void);
extern unsigned int mt6355_upmu_set_rg_vcamio_votrim(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rgs_vcamio_cal_indi(
	void);
extern unsigned int mt6355_upmu_set_rg_vcamio_oc_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcamio_ndis_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vcamio_ndis_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vcamio_stb_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcamio_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcamio_vos_cal_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vcamio_vos_cal_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vmipi_vocal(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vmipi_vosel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vmipi_vosel(
	void);
extern unsigned int mt6355_upmu_set_rg_vmipi_votrim(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rgs_vmipi_cal_indi(
	void);
extern unsigned int mt6355_upmu_set_rg_vmipi_oc_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vmipi_ndis_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vmipi_ndis_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vmipi_stb_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vmipi_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vmipi_vos_cal_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vmipi_vos_cal_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vufs18_vocal(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vufs18_vosel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vufs18_vosel(
	void);
extern unsigned int mt6355_upmu_set_rg_vufs18_votrim(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rgs_vufs18_cal_indi(
	void);
extern unsigned int mt6355_upmu_set_rg_vufs18_oc_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vufs18_ndis_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vufs18_ndis_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vufs18_stb_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vufs18_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vufs18_vos_cal_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vufs18_vos_cal_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vio18_vocal(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vio18_vosel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vio18_vosel(
	void);
extern unsigned int mt6355_upmu_set_rg_vio18_votrim(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rgs_vio18_cal_indi(
	void);
extern unsigned int mt6355_upmu_set_rg_vio18_oc_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vio18_ndis_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vio18_ndis_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vio18_stb_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vio18_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vio18_vos_cal_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vio18_vos_cal_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vxo18_vocal(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vxo18_vosel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vxo18_vosel(
	void);
extern unsigned int mt6355_upmu_set_rg_vxo18_votrim(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rgs_vxo18_cal_indi(
	void);
extern unsigned int mt6355_upmu_set_rg_vxo18_oc_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vxo18_ndis_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vxo18_ndis_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vxo18_stb_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vxo18_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vxo18_vos_cal_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vxo18_vos_cal_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vrf12_vocal(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vrf12_vosel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vrf12_vosel(
	void);
extern unsigned int mt6355_upmu_set_rg_vrf12_votrim(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rgs_vrf12_cal_indi(
	void);
extern unsigned int mt6355_upmu_set_rg_vrf12_oc_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vrf12_ndis_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vrf12_ndis_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vrf12_stb_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vrf12_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vrf12_vos_cal_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vrf12_vos_cal_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vcamd1_vocal(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcamd1_vosel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vcamd1_vosel(
	void);
extern unsigned int mt6355_upmu_set_rg_vcamd1_votrim(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rgs_vcamd1_cal_indi(
	void);
extern unsigned int mt6355_upmu_set_rg_vcamd1_oc_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcamd1_ndis_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vcamd1_ndis_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vcamd1_stb_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcamd1_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcamd1_vos_cal_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vcamd1_vos_cal_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vcamd2_vocal(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcamd2_vosel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vcamd2_vosel(
	void);
extern unsigned int mt6355_upmu_set_rg_vcamd2_votrim(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rgs_vcamd2_cal_indi(
	void);
extern unsigned int mt6355_upmu_set_rg_vcamd2_oc_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcamd2_ndis_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vcamd2_ndis_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vcamd2_stb_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcamd2_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcamd2_vos_cal_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vcamd2_vos_cal_en(
	void);
extern unsigned int mt6355_upmu_set_rg_va10_votrim(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rgs_va10_cal_indi(
	void);
extern unsigned int mt6355_upmu_set_rg_va10_oc_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_va10_ndis_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_va10_ndis_en(
	void);
extern unsigned int mt6355_upmu_set_rg_va10_stb_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_va10_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_va10_vos_cal_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_va10_vos_cal_en(
	void);
extern unsigned int mt6355_upmu_set_rg_va12_vocal(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_va12_vosel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_va12_vosel(
	void);
extern unsigned int mt6355_upmu_set_rg_va12_votrim(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rgs_va12_cal_indi(
	void);
extern unsigned int mt6355_upmu_set_rg_va12_oc_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_va12_ndis_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_va12_ndis_en(
	void);
extern unsigned int mt6355_upmu_set_rg_va12_stb_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_va12_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_va12_vos_cal_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_va12_vos_cal_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vsram_proc_oc_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vsram_proc_stb_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vsram_proc_ndis_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vsram_proc_ndis_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vsram_proc_ndis_plcur(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vsram_proc_plcur_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vsram_proc_plcur_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vsram_proc_rsv_h(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vsram_proc_rsv_l(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vsram_core_oc_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vsram_core_stb_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vsram_core_ndis_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vsram_core_ndis_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vsram_core_ndis_plcur(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vsram_core_plcur_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vsram_core_plcur_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vsram_core_rsv_h(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vsram_core_rsv_l(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vsram_gpu_oc_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vsram_gpu_stb_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vsram_gpu_ndis_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vsram_gpu_ndis_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vsram_gpu_ndis_plcur(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vsram_gpu_plcur_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vsram_gpu_plcur_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vsram_gpu_rsv_h(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vsram_gpu_rsv_l(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vsram_md_oc_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vsram_md_stb_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vsram_md_ndis_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vsram_md_ndis_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vsram_md_ndis_plcur(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vsram_md_plcur_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vsram_md_plcur_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vsram_md_rsv_h(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vsram_md_rsv_l(
	unsigned int val);
extern unsigned int mt6355_upmu_set_audaccdetauxadcswctrl(
	unsigned int val);
extern unsigned int mt6355_upmu_set_audaccdetauxadcswctrl_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audaccdetrsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_accdet_en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_accdet_seq_init(
	unsigned int val);
extern unsigned int mt6355_upmu_set_accdet_eintdet_en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_accdet_eint_seq_init(
	unsigned int val);
extern unsigned int mt6355_upmu_set_accdet_anaswctrl_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_accdet_cmp_pwm_en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_accdet_vth_pwm_en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_accdet_mbias_pwm_en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_accdet_eint_pwm_en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_accdet_cmp_pwm_idle(
	unsigned int val);
extern unsigned int mt6355_upmu_set_accdet_vth_pwm_idle(
	unsigned int val);
extern unsigned int mt6355_upmu_set_accdet_mbias_pwm_idle(
	unsigned int val);
extern unsigned int mt6355_upmu_set_accdet_eint_pwm_idle(
	unsigned int val);
extern unsigned int mt6355_upmu_set_accdet_pwm_width(
	unsigned int val);
extern unsigned int mt6355_upmu_set_accdet_pwm_thresh(
	unsigned int val);
extern unsigned int mt6355_upmu_set_accdet_rise_delay(
	unsigned int val);
extern unsigned int mt6355_upmu_set_accdet_fall_delay(
	unsigned int val);
extern unsigned int mt6355_upmu_set_accdet_debounce0(
	unsigned int val);
extern unsigned int mt6355_upmu_set_accdet_debounce1(
	unsigned int val);
extern unsigned int mt6355_upmu_set_accdet_debounce2(
	unsigned int val);
extern unsigned int mt6355_upmu_set_accdet_debounce3(
	unsigned int val);
extern unsigned int mt6355_upmu_set_accdet_debounce4(
	unsigned int val);
extern unsigned int mt6355_upmu_set_accdet_ival_cur_in(
	unsigned int val);
extern unsigned int mt6355_upmu_set_accdet_eint_ival_cur_in(
	unsigned int val);
extern unsigned int mt6355_upmu_set_accdet_ival_sam_in(
	unsigned int val);
extern unsigned int mt6355_upmu_set_accdet_eint_ival_sam_in(
	unsigned int val);
extern unsigned int mt6355_upmu_set_accdet_ival_mem_in(
	unsigned int val);
extern unsigned int mt6355_upmu_set_accdet_eint_ival_mem_in(
	unsigned int val);
extern unsigned int mt6355_upmu_set_accdet_eint_ival_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_accdet_ival_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_accdet_irq(
	void);
extern unsigned int mt6355_upmu_get_accdet_eint_irq(
	void);
extern unsigned int mt6355_upmu_set_accdet_irq_clr(
	unsigned int val);
extern unsigned int mt6355_upmu_set_accdet_eint_irq_clr(
	unsigned int val);
extern unsigned int mt6355_upmu_set_accdet_eint_irq_polarity(
	unsigned int val);
extern unsigned int mt6355_upmu_set_accdet_test_mode0(
	unsigned int val);
extern unsigned int mt6355_upmu_set_accdet_cmp_swsel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_accdet_vth_swsel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_accdet_mbias_swsel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_accdet_test_mode4(
	unsigned int val);
extern unsigned int mt6355_upmu_set_accdet_test_mode5(
	unsigned int val);
extern unsigned int mt6355_upmu_set_accdet_pwm_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_accdet_in_sw(
	unsigned int val);
extern unsigned int mt6355_upmu_set_accdet_cmp_en_sw(
	unsigned int val);
extern unsigned int mt6355_upmu_set_accdet_vth_en_sw(
	unsigned int val);
extern unsigned int mt6355_upmu_set_accdet_mbias_en_sw(
	unsigned int val);
extern unsigned int mt6355_upmu_set_accdet_pwm_en_sw(
	unsigned int val);
extern unsigned int mt6355_upmu_get_accdet_in(
	void);
extern unsigned int mt6355_upmu_get_accdet_cur_in(
	void);
extern unsigned int mt6355_upmu_get_accdet_sam_in(
	void);
extern unsigned int mt6355_upmu_get_accdet_mem_in(
	void);
extern unsigned int mt6355_upmu_get_accdet_state(
	void);
extern unsigned int mt6355_upmu_get_accdet_mbias_clk(
	void);
extern unsigned int mt6355_upmu_get_accdet_vth_clk(
	void);
extern unsigned int mt6355_upmu_get_accdet_cmp_clk(
	void);
extern unsigned int mt6355_upmu_get_da_audaccdetauxadcswctrl(
	void);
extern unsigned int mt6355_upmu_set_accdet_eint_deb_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_accdet_eint_debounce(
	unsigned int val);
extern unsigned int mt6355_upmu_set_accdet_eint_pwm_thresh(
	unsigned int val);
extern unsigned int mt6355_upmu_set_accdet_eint_pwm_width(
	unsigned int val);
extern unsigned int mt6355_upmu_set_accdet_eint_pwm_fall_delay(
	unsigned int val);
extern unsigned int mt6355_upmu_set_accdet_eint_pwm_rise_delay(
	unsigned int val);
extern unsigned int mt6355_upmu_set_accdet_test_mode13(
	unsigned int val);
extern unsigned int mt6355_upmu_set_accdet_test_mode12(
	unsigned int val);
extern unsigned int mt6355_upmu_set_accdet_test_mode11(
	unsigned int val);
extern unsigned int mt6355_upmu_set_accdet_test_mode10(
	unsigned int val);
extern unsigned int mt6355_upmu_set_accdet_eintcmpout_sw(
	unsigned int val);
extern unsigned int mt6355_upmu_set_accdet_test_mode9(
	unsigned int val);
extern unsigned int mt6355_upmu_set_accdet_test_mode8(
	unsigned int val);
extern unsigned int mt6355_upmu_set_accdet_auxadc_ctrl_sw(
	unsigned int val);
extern unsigned int mt6355_upmu_set_accdet_test_mode7(
	unsigned int val);
extern unsigned int mt6355_upmu_set_accdet_test_mode6(
	unsigned int val);
extern unsigned int mt6355_upmu_set_accdet_eintcmp_en_sw(
	unsigned int val);
extern unsigned int mt6355_upmu_get_accdet_eint_state(
	void);
extern unsigned int mt6355_upmu_get_accdet_auxadc_debounce_end(
	void);
extern unsigned int mt6355_upmu_get_accdet_auxadc_connect_pre(
	void);
extern unsigned int mt6355_upmu_get_accdet_eint_cur_in(
	void);
extern unsigned int mt6355_upmu_get_accdet_eint_sam_in(
	void);
extern unsigned int mt6355_upmu_get_accdet_eint_mem_in(
	void);
extern unsigned int mt6355_upmu_get_ad_eintcmpout(
	void);
extern unsigned int mt6355_upmu_get_da_ni_eintcmpen(
	void);
extern unsigned int mt6355_upmu_get_accdet_cur_deb(
	void);
extern unsigned int mt6355_upmu_get_accdet_eint_cur_deb(
	void);
extern unsigned int mt6355_upmu_set_accdet_rsv_con0(
	unsigned int val);
extern unsigned int mt6355_upmu_get_accdet_rsv_con1(
	void);
extern unsigned int mt6355_upmu_set_accdet_auxadc_connect_time(
	unsigned int val);
extern unsigned int mt6355_upmu_set_accdet_hwmode_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_accdet_eint_deb_out_dff(
	unsigned int val);
extern unsigned int mt6355_upmu_set_accdet_eint_reverse(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_pa(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_pdin(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_ptm(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_pwe(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_pprog(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_pwe_src(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_prog_pkey(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_rd_pkey(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_rd_trig(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_rd_rdy_bypass(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_skip_otp_out(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_rd_sw(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_otp_dout_sw(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_rd_busy(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_rd_ack(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_pa_sw(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_0_15(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_16_31(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_32_47(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_48_63(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_64_79(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_80_95(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_96_111(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_112_127(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_128_143(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_144_159(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_160_175(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_176_191(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_192_207(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_208_223(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_224_239(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_240_255(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_256_271(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_272_287(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_288_303(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_304_319(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_320_335(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_336_351(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_352_367(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_368_383(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_384_399(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_400_415(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_416_431(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_432_447(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_448_463(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_464_479(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_480_495(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_496_511(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_512_527(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_528_543(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_544_559(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_560_575(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_576_591(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_592_607(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_608_623(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_624_639(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_640_655(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_656_671(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_672_687(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_688_703(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_704_719(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_720_735(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_736_751(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_752_767(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_768_783(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_784_799(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_800_815(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_816_831(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_832_847(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_848_863(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_864_879(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_880_895(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_896_911(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_912_927(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_928_943(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_944_959(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_960_975(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_976_991(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_992_1007(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_1008_1023(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_1024_1039(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_1040_1055(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_1056_1071(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_1072_1087(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_1088_1103(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_1104_1119(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_1120_1135(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_1136_1151(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_1152_1167(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_1168_1183(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_1184_1199(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_1200_1215(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_1216_1231(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_1232_1247(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_1248_1263(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_1264_1279(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_1280_1295(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_1296_1311(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_1312_1327(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_1328_1343(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_1344_1359(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_1360_1375(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_1376_1391(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_1392_1407(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_1408_1423(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_1424_1439(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_1440_1455(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_1456_1471(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_1472_1487(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_1488_1503(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_1504_1519(
	void);
extern unsigned int mt6355_upmu_get_rg_otp_dout_1520_1535(
	void);
extern unsigned int mt6355_upmu_set_rg_otp_val_0_15(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_16_31(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_32_47(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_48_63(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_64_79(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_80_95(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_96_111(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_112_127(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_128_143(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_144_159(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_160_175(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_176_191(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_192_207(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_208_223(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_224_239(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_240_255(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_256_271(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_272_287(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_288_303(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_304_319(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_320_335(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_336_351(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_352_367(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_368_383(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_384_399(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_400_415(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_416_431(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_432_447(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_448_463(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_464_479(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_480_495(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_496_511(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_512_527(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_528_543(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_544_559(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_560_575(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_576_591(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_592_607(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_608_623(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_624_639(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_640_655(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_656_671(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_672_687(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_688_703(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_704_719(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_720_735(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_736_751(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_752_767(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_768_783(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_784_799(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_800_815(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_816_831(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_832_847(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_848_863(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_864_879(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_880_895(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_896_911(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_912_927(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_928_943(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_944_959(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_960_975(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_976_991(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_992_1007(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_1008_1023(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_1024_1039(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_1040_1055(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_1056_1071(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_1072_1087(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_1088_1103(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_1104_1119(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_1120_1135(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_1136_1151(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_1152_1167(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_1168_1183(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_1184_1199(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_1200_1215(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_1216_1231(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_1232_1247(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_1248_1263(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_1264_1279(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_1280_1295(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_1296_1311(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_1312_1327(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_1328_1343(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_1344_1359(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_1360_1375(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_1376_1391(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_1392_1407(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_1408_1423(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_1424_1439(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_1440_1455(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_1456_1471(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_1472_1487(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_1488_1503(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_1504_1519(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otp_val_1520_1535(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_extbuf1_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_extbuf1_en_m(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_extbuf2_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_extbuf2_en_m(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_extbuf3_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_extbuf3_en_m(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_extbuf4_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_extbuf4_en_m(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_bb_lpm_en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_enbb_man(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_enbb_en_m(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_clksel_man(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_clksel_en_m(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_extbuf1_ckg_man(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_extbuf1_ckg_en_m(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_extbuf2_ckg_man(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_extbuf2_ckg_en_m(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_extbuf3_ckg_man(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_extbuf3_ckg_en_m(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_extbuf4_ckg_man(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_extbuf4_ckg_en_m(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_intbuf_man(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_pbuf_en_m(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_ibuf_en_m(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_lpmbuf_man(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_lpm_prebuf_en_m(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_lpbuf_en_m(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_bblpm_cksel_m(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_en32k_man(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_en32k_m(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_xmode_man(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_xmode_m(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_strup_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_aac_fpm_time(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_aac_mode_lpm(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_aac_mode_fpm(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_en26m_offsq_en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_ldocal_en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_cbank_sync_dyn(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_26mlp_man_en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_bufldok_en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_reserved2(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_extbuf6_ckg_man(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_extbuf6_ckg_en_m(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_extbuf7_ckg_man(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_extbuf7_ckg_en_m(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_lpm_isel_man(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_fpm_isel_man(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_cdac_fpm(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_cdac_lpm(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_32kdiv_nfrac_fpm(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_cofst_fpm(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_32kdiv_nfrac_lpm(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_cofst_lpm(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_core_man(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_core_en_m(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_core_turbo_en_m(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_core_aac_en_m(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_startup_en_m(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_core_vbfpm_en_m(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_core_vblpm_en_m(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_lpmbias_en_m(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_vtcgen_en_m(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_iaac_comp_en_m(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_ifpm_comp_en_m(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_ilpm_comp_en_m(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_core_bypcas_fpm(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_core_gmx2_fpm(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_core_idac_fpm(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_aac_comp_man(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_aac_en_m(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_aac_monen_m(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_comp_en_m(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_comp_tsten_m(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_aac_hv_fpm(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_aac_ibias_fpm(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_aac_vofst_fpm(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_aac_comp_hv_fpm(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_aac_vsel_fpm(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_aac_comp_pol(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_core_bypcas_lpm(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_core_gmx2_lpm(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_core_idac_lpm(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_aac_comp_hv_lpm(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_aac_vsel_lpm(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_aac_hv_lpm(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_aac_ibias_lpm(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_aac_vofst_lpm(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_aac_fpm_swen(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_swrst(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_32kdiv_swrst(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_32kdiv_ratio_man(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_32kdiv_test_en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_cbank_sync_man(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_cbank_sync_en_m(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_ctl_sync_man(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_ctl_sync_en_m(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_ldo_man(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_ldopbuf_en_m(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_ldopbuf_vset_m(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_ldovtst_en_m(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_test_vcal_en_m(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_vbist_en_m(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_vtest_sel_mux(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_reserved3(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_extbuf6_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_extbuf6_en_m(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_extbuf7_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_extbuf7_en_m(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_bufldok_man(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_buf1ldo_cal_m(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_bufldo_cal_m(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_extbuf4_clksel_man(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_vio18pg_bufen(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_cal_en_man(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_cal_en_m(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_xo_core_osctd(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_thadc_en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_xo_sync_ckpol(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_xo_cbank_pol(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_xo_cbank_sync_byp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_xo_ctl_pol(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_xo_ctl_sync_byp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_xo_lpbuf_inv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_xo_ldopbuf_byp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_xo_ldopbuf_encl(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_xo_ldopbuf_encl(
	void);
extern unsigned int mt6355_upmu_set_rg_xo_vgbias_vset(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_xo_pbuf_iset(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_xo_ibuf_iset(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_xo_reserved4(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_xo_vow_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_xo_vow_en(
	void);
extern unsigned int mt6355_upmu_set_rg_xo_vow_div(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_xo_bufldo24_encl(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_xo_bufldo24_encl(
	void);
extern unsigned int mt6355_upmu_set_rg_xo_bufldo24_ibx2(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_xo_reserved5(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_xo_bufldo13_encl(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_xo_bufldo13_encl(
	void);
extern unsigned int mt6355_upmu_set_rg_xo_bufldo13_ibx2(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_xo_bufldo13_ix2(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_xo_lvldo_i_ctrl(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_xo_bufldo67_encl(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_xo_bufldo67_encl(
	void);
extern unsigned int mt6355_upmu_set_rg_xo_bufldo67_ibx2(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_xo_bufldo67_ix2(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_xo_lvldo_rfb(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_xo_extbuf_inv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_xo_reserved0(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_extbuf2_clksel_man(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_audio_en_m(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_xo_audio_atten(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_xo_audio_iset(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_xo_extbuf1_hd(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_xo_extbuf2_hd(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_xo_extbuf3_hd(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_xo_extbuf4_hd(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_xo_reserved8(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_xo_extbuf6_hd(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_xo_extbuf7_hd(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_extbuf1_iset_m(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_extbuf2_iset_m(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_extbuf3_iset_m(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_extbuf4_iset_m(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_reserved9(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_extbuf6_iset_m(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_extbuf7_iset_m(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_xo_lpm_prebuf_iset(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_xo_reserved1(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_thadc_en_man(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_xo_tsource_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_xo_tsource_en(
	void);
extern unsigned int mt6355_upmu_set_xo_bufldo13_vset_m(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_bufldo24_vset_m(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_bufldo67_vset_m(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_static_auxout_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_xo_auxout_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_xo_static_auxout(
	void);
extern unsigned int mt6355_upmu_set_rg_xo_pctat_comp_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_xo_pctat_comp_en(
	void);
extern unsigned int mt6355_upmu_set_rg_xo_pctat_rdeg_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_xo_gs_vtemp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_xo_heater_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_xo_corner_detect_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_xo_corner_detect_en(
	void);
extern unsigned int mt6355_upmu_set_rg_xo_corner_detect_en_man(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_xo_corner_detect_en_man(
	void);
extern unsigned int mt6355_upmu_set_rg_xo_resrved10(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_xo_corner_setting_tune(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_xo_resrved11(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ad_xo_corner_cal_done(
	void);
extern unsigned int mt6355_upmu_get_rg_ad_xo_corner_sel(
	void);
extern unsigned int mt6355_upmu_set_rg_vcdt_hv_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vcdt_hv_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vcdt_deb_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_pchr_ft_ctrl(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rgs_chrdet(
	void);
extern unsigned int mt6355_upmu_get_rgs_vcdt_lv_det(
	void);
extern unsigned int mt6355_upmu_get_rgs_vcdt_hv_det(
	void);
extern unsigned int mt6355_upmu_set_rg_vcdt_lv_vth(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcdt_hv_vth(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rgs_pchr_flag_out(
	void);
extern unsigned int mt6355_upmu_set_rg_pchr_flag_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_pchr_flag_en(
	void);
extern unsigned int mt6355_upmu_set_rg_pchr_flag_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_pchr_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_envtem_d(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_envtem_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_envtem_en(
	void);
extern unsigned int mt6355_upmu_set_rg_chr_con6_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_adcin_vsen_mux_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_adcin_vsen_mux_en(
	void);
extern unsigned int mt6355_upmu_set_rg_adcin_vsen_ext_baton_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_adcin_vsen_ext_baton_en(
	void);
extern unsigned int mt6355_upmu_set_rg_adcin_vbat_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_adcin_vbat_en(
	void);
extern unsigned int mt6355_upmu_set_rg_adcin_vsen_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_adcin_vsen_en(
	void);
extern unsigned int mt6355_upmu_set_rg_adcin_chr_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_adcin_chr_en(
	void);
extern unsigned int mt6355_upmu_set_da_qi_baton_lt_en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_da_qi_bgr_ext_buf_en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_baton_tdet_en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_bgr_unchop(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_bgr_unchop_ph(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_bgr_rsel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_bgr_trim_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_bgr_trim_en(
	void);
extern unsigned int mt6355_upmu_set_rg_bgr_test_rstb(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_bgr_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_bgr_test_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_bgr_test_en(
	void);
extern unsigned int mt6355_upmu_set_rg_vcdt_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_baton_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_baton_en(
	void);
extern unsigned int mt6355_upmu_set_rg_baton_ht_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_hw_vth1(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_hw_vth2(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_hw_vth_ctrl(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_otg_bvalid_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_otg_bvalid_en(
	void);
extern unsigned int mt6355_upmu_set_rg_uvlo_vthl(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_uvlo_vh_lat(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_lbat_int_vth(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_pchr_rv(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rgs_baton_undet(
	void);
extern unsigned int mt6355_upmu_get_rgs_otg_bvalid_det(
	void);
extern unsigned int mt6355_upmu_get_rgs_chr_ldo_det(
	void);
extern unsigned int mt6355_upmu_get_rgs_baton_hv(
	void);
extern unsigned int mt6355_upmu_set_rg_pchr_ana3_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcdt_uvlo_vth(
	unsigned int val);
extern unsigned int mt6355_upmu_set_gpio_dir0(
	unsigned int val);
extern unsigned int mt6355_upmu_set_gpio_pullen0(
	unsigned int val);
extern unsigned int mt6355_upmu_set_gpio_pullsel0(
	unsigned int val);
extern unsigned int mt6355_upmu_set_gpio_dinv0(
	unsigned int val);
extern unsigned int mt6355_upmu_set_gpio_dout0(
	unsigned int val);
extern unsigned int mt6355_upmu_get_gpio_pi0(
	void);
extern unsigned int mt6355_upmu_get_gpio_poe0(
	void);
extern unsigned int mt6355_upmu_set_gpio0_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_gpio1_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_gpio2_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_gpio3_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_gpio4_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_gpio5_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_gpio6_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_gpio7_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_gpio8_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_gpio9_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_gpio10_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_gpio11_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_gpio12_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_gpio13_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_gpio_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rtc_sec_dummy(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rtc_dummy(
	unsigned int val);
extern unsigned int mt6355_upmu_set_eosc_cali_start(
	unsigned int val);
extern unsigned int mt6355_upmu_set_eosc_cali_td(
	unsigned int val);
extern unsigned int mt6355_upmu_set_eosc_cali_test(
	unsigned int val);
extern unsigned int mt6355_upmu_set_eosc_cali_dcxo_rdy_td(
	unsigned int val);
extern unsigned int mt6355_upmu_set_frc_vtcxo0_on(
	unsigned int val);
extern unsigned int mt6355_upmu_set_eosc_cali_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_get_mix_eosc32_stp_lpdtb(
	void);
extern unsigned int mt6355_upmu_set_mix_eosc32_stp_lpden(
	unsigned int val);
extern unsigned int mt6355_upmu_set_mix_xosc32_stp_pwdb(
	unsigned int val);
extern unsigned int mt6355_upmu_get_mix_xosc32_stp_lpdtb(
	void);
extern unsigned int mt6355_upmu_set_mix_xosc32_stp_lpden(
	unsigned int val);
extern unsigned int mt6355_upmu_set_mix_xosc32_stp_lpdrst(
	unsigned int val);
extern unsigned int mt6355_upmu_set_mix_xosc32_stp_cali(
	unsigned int val);
extern unsigned int mt6355_upmu_set_stmp_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_mix_eosc32_stp_chop_en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_mix_dcxo_stp_lvsh_en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_mix_pmu_stp_ddlo_vrtc(
	unsigned int val);
extern unsigned int mt6355_upmu_set_mix_pmu_stp_ddlo_vrtc_en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_mix_rtc_stp_xosc32_enb(
	unsigned int val);
extern unsigned int mt6355_upmu_set_mix_dcxo_stp_test_deglitch_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_mix_eosc32_stp_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_mix_eosc32_vct_en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_mix_eosc32_opt(
	unsigned int val);
extern unsigned int mt6355_upmu_set_mix_dcxo_stp_lvsh_en_int(
	unsigned int val);
extern unsigned int mt6355_upmu_set_mix_rtc_gpio_coredetb(
	unsigned int val);
extern unsigned int mt6355_upmu_set_mix_rtc_gpio_f32kob(
	unsigned int val);
extern unsigned int mt6355_upmu_set_mix_rtc_gpio_gpo(
	unsigned int val);
extern unsigned int mt6355_upmu_set_mix_rtc_gpio_oe(
	unsigned int val);
extern unsigned int mt6355_upmu_get_mix_rtc_stp_debug_out(
	void);
extern unsigned int mt6355_upmu_set_mix_rtc_stp_debug_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_mix_rtc_stp_k_eosc32_en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_mix_rtc_stp_embck_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_mix_stp_bbwakeup(
	unsigned int val);
extern unsigned int mt6355_upmu_get_mix_stp_rtc_ddlo(
	void);
extern unsigned int mt6355_upmu_get_mix_rtc_xosc32_enb(
	void);
extern unsigned int mt6355_upmu_set_mix_efuse_xosc32_enb_opt(
	unsigned int val);
extern unsigned int mt6355_upmu_set_vrtc_pwm_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_vrtc_pwm_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_vrtc_pwm_l_duty(
	unsigned int val);
extern unsigned int mt6355_upmu_set_vrtc_pwm_h_duty(
	unsigned int val);
extern unsigned int mt6355_upmu_set_vrtc_cap_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_bif_command_0(
	unsigned int val);
extern unsigned int mt6355_upmu_set_bif_command_1(
	unsigned int val);
extern unsigned int mt6355_upmu_set_bif_command_2(
	unsigned int val);
extern unsigned int mt6355_upmu_set_bif_command_3(
	unsigned int val);
extern unsigned int mt6355_upmu_set_bif_command_4(
	unsigned int val);
extern unsigned int mt6355_upmu_set_bif_command_5(
	unsigned int val);
extern unsigned int mt6355_upmu_set_bif_command_6(
	unsigned int val);
extern unsigned int mt6355_upmu_set_bif_command_7(
	unsigned int val);
extern unsigned int mt6355_upmu_set_bif_command_8(
	unsigned int val);
extern unsigned int mt6355_upmu_set_bif_command_9(
	unsigned int val);
extern unsigned int mt6355_upmu_set_bif_command_10(
	unsigned int val);
extern unsigned int mt6355_upmu_set_bif_command_11(
	unsigned int val);
extern unsigned int mt6355_upmu_set_bif_command_12(
	unsigned int val);
extern unsigned int mt6355_upmu_set_bif_command_13(
	unsigned int val);
extern unsigned int mt6355_upmu_set_bif_command_14(
	unsigned int val);
extern unsigned int mt6355_upmu_set_bif_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_bif_command_type(
	unsigned int val);
extern unsigned int mt6355_upmu_set_bif_trasfer_num(
	unsigned int val);
extern unsigned int mt6355_upmu_set_bif_logic_0_set(
	unsigned int val);
extern unsigned int mt6355_upmu_set_bif_logic_1_set(
	unsigned int val);
extern unsigned int mt6355_upmu_set_bif_stop_set(
	unsigned int val);
extern unsigned int mt6355_upmu_set_bif_debounce_en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_bif_read_expect_num(
	unsigned int val);
extern unsigned int mt6355_upmu_set_bif_trasact_trigger(
	unsigned int val);
extern unsigned int mt6355_upmu_get_bif_data_num(
	void);
extern unsigned int mt6355_upmu_get_bif_response(
	void);
extern unsigned int mt6355_upmu_get_bif_data_0(
	void);
extern unsigned int mt6355_upmu_get_bif_ack_0(
	void);
extern unsigned int mt6355_upmu_get_bif_error_0(
	void);
extern unsigned int mt6355_upmu_get_bif_data_1(
	void);
extern unsigned int mt6355_upmu_get_bif_ack_1(
	void);
extern unsigned int mt6355_upmu_get_bif_error_1(
	void);
extern unsigned int mt6355_upmu_get_bif_data_2(
	void);
extern unsigned int mt6355_upmu_get_bif_ack_2(
	void);
extern unsigned int mt6355_upmu_get_bif_error_2(
	void);
extern unsigned int mt6355_upmu_get_bif_data_3(
	void);
extern unsigned int mt6355_upmu_get_bif_ack_3(
	void);
extern unsigned int mt6355_upmu_get_bif_error_3(
	void);
extern unsigned int mt6355_upmu_get_bif_data_4(
	void);
extern unsigned int mt6355_upmu_get_bif_ack_4(
	void);
extern unsigned int mt6355_upmu_get_bif_error_4(
	void);
extern unsigned int mt6355_upmu_get_bif_data_5(
	void);
extern unsigned int mt6355_upmu_get_bif_ack_5(
	void);
extern unsigned int mt6355_upmu_get_bif_error_5(
	void);
extern unsigned int mt6355_upmu_get_bif_data_6(
	void);
extern unsigned int mt6355_upmu_get_bif_ack_6(
	void);
extern unsigned int mt6355_upmu_get_bif_error_6(
	void);
extern unsigned int mt6355_upmu_get_bif_data_7(
	void);
extern unsigned int mt6355_upmu_get_bif_ack_7(
	void);
extern unsigned int mt6355_upmu_get_bif_error_7(
	void);
extern unsigned int mt6355_upmu_get_bif_data_8(
	void);
extern unsigned int mt6355_upmu_get_bif_ack_8(
	void);
extern unsigned int mt6355_upmu_get_bif_error_8(
	void);
extern unsigned int mt6355_upmu_get_bif_data_9(
	void);
extern unsigned int mt6355_upmu_get_bif_ack_9(
	void);
extern unsigned int mt6355_upmu_get_bif_error_9(
	void);
extern unsigned int mt6355_upmu_set_bif_test_mode0(
	unsigned int val);
extern unsigned int mt6355_upmu_set_bif_test_mode1(
	unsigned int val);
extern unsigned int mt6355_upmu_set_bif_test_mode2(
	unsigned int val);
extern unsigned int mt6355_upmu_set_bif_test_mode3(
	unsigned int val);
extern unsigned int mt6355_upmu_set_bif_test_mode4(
	unsigned int val);
extern unsigned int mt6355_upmu_set_bif_test_mode5(
	unsigned int val);
extern unsigned int mt6355_upmu_set_bif_test_mode6(
	unsigned int val);
extern unsigned int mt6355_upmu_set_bif_test_mode7(
	unsigned int val);
extern unsigned int mt6355_upmu_set_bif_test_mode8(
	unsigned int val);
extern unsigned int mt6355_upmu_set_bif_bat_lost_sw(
	unsigned int val);
extern unsigned int mt6355_upmu_set_bif_rx_data_sw(
	unsigned int val);
extern unsigned int mt6355_upmu_set_bif_tx_data_sw(
	unsigned int val);
extern unsigned int mt6355_upmu_set_bif_rx_en_sw(
	unsigned int val);
extern unsigned int mt6355_upmu_set_bif_tx_en_sw(
	unsigned int val);
extern unsigned int mt6355_upmu_set_bif_back_normal(
	unsigned int val);
extern unsigned int mt6355_upmu_set_bif_irq_clr(
	unsigned int val);
extern unsigned int mt6355_upmu_get_bif_irq(
	void);
extern unsigned int mt6355_upmu_get_bif_timeout(
	void);
extern unsigned int mt6355_upmu_get_bif_bat_undet(
	void);
extern unsigned int mt6355_upmu_get_bif_total_valid(
	void);
extern unsigned int mt6355_upmu_get_bif_bus_status(
	void);
extern unsigned int mt6355_upmu_set_bif_power_up_count(
	unsigned int val);
extern unsigned int mt6355_upmu_set_bif_power_up(
	unsigned int val);
extern unsigned int mt6355_upmu_get_bif_rx_error_unknown(
	void);
extern unsigned int mt6355_upmu_get_bif_rx_error_insuff(
	void);
extern unsigned int mt6355_upmu_get_bif_rx_error_lowphase(
	void);
extern unsigned int mt6355_upmu_get_bif_rx_state(
	void);
extern unsigned int mt6355_upmu_get_bif_flow_ctl_state(
	void);
extern unsigned int mt6355_upmu_get_bif_tx_state(
	void);
extern unsigned int mt6355_upmu_get_ad_qi_bif_rx_data(
	void);
extern unsigned int mt6355_upmu_get_da_qi_bif_rx_en(
	void);
extern unsigned int mt6355_upmu_get_da_qi_bif_tx_data(
	void);
extern unsigned int mt6355_upmu_get_da_qi_bif_tx_en(
	void);
extern unsigned int mt6355_upmu_get_bif_tx_data_fianl(
	void);
extern unsigned int mt6355_upmu_get_bif_rx_data_sampling(
	void);
extern unsigned int mt6355_upmu_get_bif_rx_data_recovery(
	void);
extern unsigned int mt6355_upmu_set_rg_baton_ht_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_baton_ht_en(
	void);
extern unsigned int mt6355_upmu_set_rg_baton_ht_en_dly_time(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_baton_ht_en_dly_time(
	void);
extern unsigned int mt6355_upmu_set_bif_timeout_set(
	unsigned int val);
extern unsigned int mt6355_upmu_set_bif_rx_deg_wnd(
	unsigned int val);
extern unsigned int mt6355_upmu_set_bif_rx_deg_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_bif_rsv1(
	void);
extern unsigned int mt6355_upmu_set_bif_rsv0(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_qi_baton_ht_en(
	void);
extern unsigned int mt6355_upmu_set_rg_baton_debounce_wnd(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_baton_debounce_thd(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_baton_vbif_stb(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_baton_chrdet_test_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_baton_undet_test_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_baton_auxadc_test_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_baton_fgadc_test_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_baton_rtc_test_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_baton_bif_test_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_baton_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_baton_chrdet_sw(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_baton_undet_sw(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_baton_auxadc_sw(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_baton_fgadc_sw(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_baton_rtc_sw(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_baton_bif_sw(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_baton_rsv_sw(
	unsigned int val);
extern unsigned int mt6355_upmu_get_baton_status(
	void);
extern unsigned int mt6355_upmu_get_baton_auxadc_set(
	void);
extern unsigned int mt6355_upmu_get_baton_deb_valid(
	void);
extern unsigned int mt6355_upmu_get_baton_bif_status(
	void);
extern unsigned int mt6355_upmu_get_baton_rtc_status(
	void);
extern unsigned int mt6355_upmu_get_baton_fgadc_status(
	void);
extern unsigned int mt6355_upmu_get_baton_auxadc_trig(
	void);
extern unsigned int mt6355_upmu_get_baton_chrdet_deb(
	void);
extern unsigned int mt6355_upmu_get_baton_ivgen_enb(
	void);
extern unsigned int mt6355_upmu_get_baton_vbif28_stb(
	void);
extern unsigned int mt6355_upmu_get_baton_vbif28_en(
	void);
extern unsigned int mt6355_upmu_get_baton_rsv_0(
	void);
extern unsigned int mt6355_upmu_set_rg_bif_batid_sw_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_bif_batid_sw_en(
	void);
extern unsigned int mt6355_upmu_set_fg_on(
	unsigned int val);
extern unsigned int mt6355_upmu_set_fg_cal(
	unsigned int val);
extern unsigned int mt6355_upmu_set_fg_autocalrate(
	unsigned int val);
extern unsigned int mt6355_upmu_set_fg_son_slp_en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_fg_soff_slp_en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_fg_zcv_det_en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_fg_auxadc_r(
	unsigned int val);
extern unsigned int mt6355_upmu_set_fg_iavg_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_fg_sw_read_pre(
	unsigned int val);
extern unsigned int mt6355_upmu_set_fg_sw_rstclr(
	unsigned int val);
extern unsigned int mt6355_upmu_set_fg_sw_cr(
	unsigned int val);
extern unsigned int mt6355_upmu_set_fg_sw_clear(
	unsigned int val);
extern unsigned int mt6355_upmu_set_fg_offset_rst(
	unsigned int val);
extern unsigned int mt6355_upmu_set_fg_time_rst(
	unsigned int val);
extern unsigned int mt6355_upmu_set_fg_charge_rst(
	unsigned int val);
extern unsigned int mt6355_upmu_set_fg_n_charge_rst(
	unsigned int val);
extern unsigned int mt6355_upmu_set_fg_soff_rst(
	unsigned int val);
extern unsigned int mt6355_upmu_get_fg_latchdata_st(
	void);
extern unsigned int mt6355_upmu_get_event_fg_bat0_l(
	void);
extern unsigned int mt6355_upmu_get_event_fg_bat0_h(
	void);
extern unsigned int mt6355_upmu_get_event_fg_bat1_l(
	void);
extern unsigned int mt6355_upmu_get_event_fg_bat1_h(
	void);
extern unsigned int mt6355_upmu_get_event_fg_cur_l(
	void);
extern unsigned int mt6355_upmu_get_event_fg_cur_h(
	void);
extern unsigned int mt6355_upmu_get_event_fg_iavg_l(
	void);
extern unsigned int mt6355_upmu_get_event_fg_iavg_h(
	void);
extern unsigned int mt6355_upmu_get_event_fg_n_charge_l(
	void);
extern unsigned int mt6355_upmu_get_event_fg_time_h(
	void);
extern unsigned int mt6355_upmu_get_event_fg_discharge(
	void);
extern unsigned int mt6355_upmu_get_event_fg_charge(
	void);
extern unsigned int mt6355_upmu_get_event_fg_zcv(
	void);
extern unsigned int mt6355_upmu_set_fg_osr1(
	unsigned int val);
extern unsigned int mt6355_upmu_set_fg_osr2(
	unsigned int val);
extern unsigned int mt6355_upmu_set_fg_fir1bypass(
	unsigned int val);
extern unsigned int mt6355_upmu_set_fg_fir2bypass(
	unsigned int val);
extern unsigned int mt6355_upmu_set_fg_adj_offset_en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_fg_adc_autorst(
	unsigned int val);
extern unsigned int mt6355_upmu_get_fg_adc_rstdetect(
	void);
extern unsigned int mt6355_upmu_set_fg_va18_aon(
	unsigned int val);
extern unsigned int mt6355_upmu_set_fg_va18_aoff(
	unsigned int val);
extern unsigned int mt6355_upmu_set_fg_son_sw(
	unsigned int val);
extern unsigned int mt6355_upmu_set_fg_son_sw_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_fgadc_con4_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_fg_rstb_status(
	unsigned int val);
extern unsigned int mt6355_upmu_get_fg_r_curr(
	void);
extern unsigned int mt6355_upmu_get_fg_current_out(
	void);
extern unsigned int mt6355_upmu_set_fg_cur_lth(
	unsigned int val);
extern unsigned int mt6355_upmu_set_fg_cur_hth(
	unsigned int val);
extern unsigned int mt6355_upmu_get_fg_cic2(
	void);
extern unsigned int mt6355_upmu_get_fg_car_31_16(
	void);
extern unsigned int mt6355_upmu_get_fg_car_15_00(
	void);
extern unsigned int mt6355_upmu_set_fg_bat0_lth_31_16(
	unsigned int val);
extern unsigned int mt6355_upmu_set_fg_bat0_lth_15_14(
	unsigned int val);
extern unsigned int mt6355_upmu_set_fg_bat0_hth_31_16(
	unsigned int val);
extern unsigned int mt6355_upmu_set_fg_bat0_hth_15_14(
	unsigned int val);
extern unsigned int mt6355_upmu_set_fg_bat1_lth_31_16(
	unsigned int val);
extern unsigned int mt6355_upmu_set_fg_bat1_lth_15_14(
	unsigned int val);
extern unsigned int mt6355_upmu_set_fg_bat1_hth_31_16(
	unsigned int val);
extern unsigned int mt6355_upmu_set_fg_bat1_hth_15_14(
	unsigned int val);
extern unsigned int mt6355_upmu_get_fg_ncar_31_16(
	void);
extern unsigned int mt6355_upmu_get_fg_ncar_15_00(
	void);
extern unsigned int mt6355_upmu_set_fg_n_charge_lth_31_16(
	unsigned int val);
extern unsigned int mt6355_upmu_set_fg_n_charge_lth_15_14(
	unsigned int val);
extern unsigned int mt6355_upmu_get_fg_iavg_27_16(
	void);
extern unsigned int mt6355_upmu_get_fg_iavg_vld(
	void);
extern unsigned int mt6355_upmu_get_fg_iavg_15_00(
	void);
extern unsigned int mt6355_upmu_set_fg_iavg_lth_28_16(
	unsigned int val);
extern unsigned int mt6355_upmu_set_fg_iavg_lth_15_00(
	unsigned int val);
extern unsigned int mt6355_upmu_set_fg_iavg_hth_28_16(
	unsigned int val);
extern unsigned int mt6355_upmu_set_fg_iavg_hth_15_00(
	unsigned int val);
extern unsigned int mt6355_upmu_get_fg_nter_29_16(
	void);
extern unsigned int mt6355_upmu_get_fg_nter_15_00(
	void);
extern unsigned int mt6355_upmu_set_fg_time_hth_29_16(
	unsigned int val);
extern unsigned int mt6355_upmu_set_fg_time_hth_15_00(
	unsigned int val);
extern unsigned int mt6355_upmu_get_fg_offset(
	void);
extern unsigned int mt6355_upmu_set_fg_adjust_offset_value(
	unsigned int val);
extern unsigned int mt6355_upmu_set_fg_gain(
	unsigned int val);
extern unsigned int mt6355_upmu_set_fg_son_slp_cur_th(
	unsigned int val);
extern unsigned int mt6355_upmu_set_fg_son_slp_time(
	unsigned int val);
extern unsigned int mt6355_upmu_set_fg_son_det_time(
	unsigned int val);
extern unsigned int mt6355_upmu_set_fg_fp_ftime(
	unsigned int val);
extern unsigned int mt6355_upmu_set_fg_soff_slp_cur_th(
	unsigned int val);
extern unsigned int mt6355_upmu_set_fg_soff_slp_time(
	unsigned int val);
extern unsigned int mt6355_upmu_set_fg_soff_det_time(
	unsigned int val);
extern unsigned int mt6355_upmu_get_fg_soff_time_29_16(
	void);
extern unsigned int mt6355_upmu_get_fg_soff(
	void);
extern unsigned int mt6355_upmu_get_fg_soff_time_15_00(
	void);
extern unsigned int mt6355_upmu_set_fg_pwr_time0(
	unsigned int val);
extern unsigned int mt6355_upmu_set_fg_pwr_time1(
	unsigned int val);
extern unsigned int mt6355_upmu_set_fg_pwr_time2(
	unsigned int val);
extern unsigned int mt6355_upmu_set_fg_zcv_det_iv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_fg_zcv_car_th_30_16(
	unsigned int val);
extern unsigned int mt6355_upmu_set_fg_zcv_car_th_15_00(
	unsigned int val);
extern unsigned int mt6355_upmu_get_fg_zcv_car_31_16(
	void);
extern unsigned int mt6355_upmu_get_fg_zcv_car_15_00(
	void);
extern unsigned int mt6355_upmu_get_fg_zcv_curr(
	void);
extern unsigned int mt6355_upmu_set_rg_fganalogtest_3_1(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_fgrintmode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_spare(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_fg_offset_swap(
	unsigned int val);
extern unsigned int mt6355_upmu_set_fg_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_fg_rst_sw(
	unsigned int val);
extern unsigned int mt6355_upmu_set_fg_fgcal_en_sw(
	unsigned int val);
extern unsigned int mt6355_upmu_set_fg_fgadc_en_sw(
	unsigned int val);
extern unsigned int mt6355_upmu_set_fg_rng_bit_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_fg_rng_bit_sw(
	unsigned int val);
extern unsigned int mt6355_upmu_set_fg_rng_en_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_fg_rng_en_sw(
	unsigned int val);
extern unsigned int mt6355_upmu_set_fg_dwa_t0(
	unsigned int val);
extern unsigned int mt6355_upmu_set_fg_dwa_t1(
	unsigned int val);
extern unsigned int mt6355_upmu_set_fg_dwa_rst_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_fg_dwa_rst_sw(
	unsigned int val);
extern unsigned int mt6355_upmu_get_fg_dwa_rst(
	void);
extern unsigned int mt6355_upmu_set_fg_rsv_con0_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_fg_rsv_con1_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_fg_rsv_con2_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_fg_rsv_con3_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_system_info_con0(
	unsigned int val);
extern unsigned int mt6355_upmu_set_system_info_con1(
	unsigned int val);
extern unsigned int mt6355_upmu_set_system_info_con2(
	unsigned int val);
extern unsigned int mt6355_upmu_set_system_info_con3(
	unsigned int val);
extern unsigned int mt6355_upmu_set_system_info_con4(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_fgadc_gainerror_cal(
	unsigned int val);
extern unsigned int mt6355_upmu_get_auxadc_adc_out_ch0(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_rdy_ch0(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_out_ch1(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_rdy_ch1(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_out_ch2(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_rdy_ch2(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_out_ch3(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_rdy_ch3(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_out_ch4(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_rdy_ch4(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_out_ch5(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_rdy_ch5(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_out_ch6(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_rdy_ch6(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_out_ch7(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_rdy_ch7(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_out_ch8(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_rdy_ch8(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_out_ch9(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_rdy_ch9(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_out_ch10(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_rdy_ch10(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_out_ch11(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_rdy_ch11(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_out_ch12_15(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_rdy_ch12_15(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_out_thr_hw(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_rdy_thr_hw(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_out_lbat(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_rdy_lbat(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_out_lbat2(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_rdy_lbat2(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_out_ch7_by_gps(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_rdy_ch7_by_gps(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_out_ch7_by_md(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_rdy_ch7_by_md(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_out_ch7_by_ap(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_rdy_ch7_by_ap(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_out_ch4_by_md(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_rdy_ch4_by_md(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_out_pwron_pchr(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_rdy_pwron_pchr(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_out_pwron_swchr(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_rdy_pwron_swchr(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_out_wakeup_pchr(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_rdy_wakeup_pchr(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_out_wakeup_swchr(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_rdy_wakeup_swchr(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_out_ch0_by_md(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_rdy_ch0_by_md(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_out_ch0_by_ap(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_rdy_ch0_by_ap(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_out_ch1_by_md(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_rdy_ch1_by_md(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_out_ch1_by_ap(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_rdy_ch1_by_ap(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_out_bat_temp(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_rdy_bat_temp(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_out_fgadc_pchr(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_rdy_fgadc_pchr(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_out_fgadc_swchr(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_rdy_fgadc_swchr(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_out_bat_plugin_pchr(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_rdy_bat_plugin_pchr(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_out_bat_plugin_swchr(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_rdy_bat_plugin_swchr(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_out_imp(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_rdy_imp(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_out_imp_avg(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_rdy_imp_avg(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_out_raw(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_out_mdrt(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_rdy_mdrt(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_out_mdbg(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_rdy_mdbg(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_out_jeita(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_rdy_jeita(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_out_dcxo_by_gps(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_rdy_dcxo_by_gps(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_out_dcxo_by_md(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_rdy_dcxo_by_md(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_out_dcxo_by_ap(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_rdy_dcxo_by_ap(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_out_dcxo_mdrt(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_rdy_dcxo_mdrt(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_out_nag(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_rdy_nag(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_out_batid(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_rdy_batid(
	void);
extern unsigned int mt6355_upmu_get_auxadc_buf_out_00(
	void);
extern unsigned int mt6355_upmu_get_auxadc_buf_rdy_00(
	void);
extern unsigned int mt6355_upmu_get_auxadc_buf_out_01(
	void);
extern unsigned int mt6355_upmu_get_auxadc_buf_rdy_01(
	void);
extern unsigned int mt6355_upmu_get_auxadc_buf_out_02(
	void);
extern unsigned int mt6355_upmu_get_auxadc_buf_rdy_02(
	void);
extern unsigned int mt6355_upmu_get_auxadc_buf_out_03(
	void);
extern unsigned int mt6355_upmu_get_auxadc_buf_rdy_03(
	void);
extern unsigned int mt6355_upmu_get_auxadc_buf_out_04(
	void);
extern unsigned int mt6355_upmu_get_auxadc_buf_rdy_04(
	void);
extern unsigned int mt6355_upmu_get_auxadc_buf_out_05(
	void);
extern unsigned int mt6355_upmu_get_auxadc_buf_rdy_05(
	void);
extern unsigned int mt6355_upmu_get_auxadc_buf_out_06(
	void);
extern unsigned int mt6355_upmu_get_auxadc_buf_rdy_06(
	void);
extern unsigned int mt6355_upmu_get_auxadc_buf_out_07(
	void);
extern unsigned int mt6355_upmu_get_auxadc_buf_rdy_07(
	void);
extern unsigned int mt6355_upmu_get_auxadc_buf_out_08(
	void);
extern unsigned int mt6355_upmu_get_auxadc_buf_rdy_08(
	void);
extern unsigned int mt6355_upmu_get_auxadc_buf_out_09(
	void);
extern unsigned int mt6355_upmu_get_auxadc_buf_rdy_09(
	void);
extern unsigned int mt6355_upmu_get_auxadc_buf_out_10(
	void);
extern unsigned int mt6355_upmu_get_auxadc_buf_rdy_10(
	void);
extern unsigned int mt6355_upmu_get_auxadc_buf_out_11(
	void);
extern unsigned int mt6355_upmu_get_auxadc_buf_rdy_11(
	void);
extern unsigned int mt6355_upmu_get_auxadc_buf_out_12(
	void);
extern unsigned int mt6355_upmu_get_auxadc_buf_rdy_12(
	void);
extern unsigned int mt6355_upmu_get_auxadc_buf_out_13(
	void);
extern unsigned int mt6355_upmu_get_auxadc_buf_rdy_13(
	void);
extern unsigned int mt6355_upmu_get_auxadc_buf_out_14(
	void);
extern unsigned int mt6355_upmu_get_auxadc_buf_rdy_14(
	void);
extern unsigned int mt6355_upmu_get_auxadc_buf_out_15(
	void);
extern unsigned int mt6355_upmu_get_auxadc_buf_rdy_15(
	void);
extern unsigned int mt6355_upmu_get_auxadc_buf_out_16(
	void);
extern unsigned int mt6355_upmu_get_auxadc_buf_rdy_16(
	void);
extern unsigned int mt6355_upmu_get_auxadc_buf_out_17(
	void);
extern unsigned int mt6355_upmu_get_auxadc_buf_rdy_17(
	void);
extern unsigned int mt6355_upmu_get_auxadc_buf_out_18(
	void);
extern unsigned int mt6355_upmu_get_auxadc_buf_rdy_18(
	void);
extern unsigned int mt6355_upmu_get_auxadc_buf_out_19(
	void);
extern unsigned int mt6355_upmu_get_auxadc_buf_rdy_19(
	void);
extern unsigned int mt6355_upmu_get_auxadc_buf_out_20(
	void);
extern unsigned int mt6355_upmu_get_auxadc_buf_rdy_20(
	void);
extern unsigned int mt6355_upmu_get_auxadc_buf_out_21(
	void);
extern unsigned int mt6355_upmu_get_auxadc_buf_rdy_21(
	void);
extern unsigned int mt6355_upmu_get_auxadc_buf_out_22(
	void);
extern unsigned int mt6355_upmu_get_auxadc_buf_rdy_22(
	void);
extern unsigned int mt6355_upmu_get_auxadc_buf_out_23(
	void);
extern unsigned int mt6355_upmu_get_auxadc_buf_rdy_23(
	void);
extern unsigned int mt6355_upmu_get_auxadc_buf_out_24(
	void);
extern unsigned int mt6355_upmu_get_auxadc_buf_rdy_24(
	void);
extern unsigned int mt6355_upmu_get_auxadc_buf_out_25(
	void);
extern unsigned int mt6355_upmu_get_auxadc_buf_rdy_25(
	void);
extern unsigned int mt6355_upmu_get_auxadc_buf_out_26(
	void);
extern unsigned int mt6355_upmu_get_auxadc_buf_rdy_26(
	void);
extern unsigned int mt6355_upmu_get_auxadc_buf_out_27(
	void);
extern unsigned int mt6355_upmu_get_auxadc_buf_rdy_27(
	void);
extern unsigned int mt6355_upmu_get_auxadc_buf_out_28(
	void);
extern unsigned int mt6355_upmu_get_auxadc_buf_rdy_28(
	void);
extern unsigned int mt6355_upmu_get_auxadc_buf_out_29(
	void);
extern unsigned int mt6355_upmu_get_auxadc_buf_rdy_29(
	void);
extern unsigned int mt6355_upmu_get_auxadc_buf_out_30(
	void);
extern unsigned int mt6355_upmu_get_auxadc_buf_rdy_30(
	void);
extern unsigned int mt6355_upmu_get_auxadc_buf_out_31(
	void);
extern unsigned int mt6355_upmu_get_auxadc_buf_rdy_31(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_busy_in(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_busy_in_lbat(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_busy_in_lbat2(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_busy_in_bat_temp(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_busy_in_wakeup(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_busy_in_dcxo_mdrt(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_busy_in_dcxo_gps_ap(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_busy_in_dcxo_gps_md(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_busy_in_dcxo_gps(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_busy_in_jeita(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_busy_in_mdrt(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_busy_in_mdbg(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_busy_in_share(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_busy_in_imp(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_busy_in_fgadc_pchr(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_busy_in_fgadc_swchr(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_busy_in_gps_ap(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_busy_in_gps_md(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_busy_in_gps(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_busy_in_thr_hw(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_busy_in_thr_md(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_busy_in_bat_plugin_pchr(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_busy_in_bat_plugin_swchr(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_busy_in_batid(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_busy_in_pwron(
	void);
extern unsigned int mt6355_upmu_get_auxadc_adc_busy_in_nag(
	void);
extern unsigned int mt6355_upmu_set_auxadc_rqst_ch0(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_rqst_ch1(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_rqst_ch2(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_rqst_ch3(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_rqst_ch4(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_rqst_ch5(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_rqst_ch6(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_rqst_ch7(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_rqst_ch8(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_rqst_ch9(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_rqst_ch10(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_rqst_ch11(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_rqst_ch12(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_rqst_ch13(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_rqst_ch14(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_rqst_ch15(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_rqst_ch0_by_md(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_rqst_ch1_by_md(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_rqst_rsv0(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_rqst_batid(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_rqst_ch4_by_md(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_rqst_ch7_by_md(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_rqst_ch7_by_gps(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_rqst_dcxo_by_md(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_rqst_dcxo_by_gps(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_rqst_rsv1(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_ck_on_extd(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_srclken_src_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_adc_pwdb(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_adc_pwdb_swctrl(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_strup_ck_on_enb(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_srclken_ck_en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_ck_aon_gps(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_ck_aon_md(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_ck_aon(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_avg_num_small(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_avg_num_large(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_spl_num(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_avg_num_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_avg_num_sel_share(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_avg_num_sel_lbat(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_avg_num_sel_bat_temp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_avg_num_sel_wakeup(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_spl_num_large(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_spl_num_sleep(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_spl_num_sleep_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_spl_num_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_spl_num_sel_share(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_spl_num_sel_lbat(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_spl_num_sel_bat_temp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_spl_num_sel_wakeup(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_spl_num_ch0(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_spl_num_ch3(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_spl_num_ch7(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_avg_num_lbat(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_avg_num_ch7(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_avg_num_ch3(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_avg_num_ch0(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_avg_num_hpc(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_avg_num_dcxo(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_trim_ch0_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_trim_ch1_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_trim_ch2_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_trim_ch3_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_trim_ch4_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_trim_ch5_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_trim_ch6_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_trim_ch7_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_trim_ch8_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_trim_ch9_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_trim_ch10_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_trim_ch11_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_adc_2s_comp_enb(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_adc_trim_comp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_sw_gain_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_sw_offset_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_rng_en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_test_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_bit_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_start_sw(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_start_swctrl(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_ts_vbe_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_ts_vbe_sel_swctrl(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_vbuf_en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_vbuf_en_swctrl(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_out_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_da_dac(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_da_dac_swctrl(
	unsigned int val);
extern unsigned int mt6355_upmu_get_ad_auxadc_comp(
	void);
extern unsigned int mt6355_upmu_set_rg_auxadc_cali(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_aux_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vbuf_byp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vbuf_calen(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vbuf_exten(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_adcin_vsen_en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_adcin_vbat_en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_adcin_vsen_mux_en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_adcin_vsen_ext_baton_en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_adcin_chr_en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_adcin_baton_tdet_en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_accdet_anaswctrl_en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_xo_thadc_en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_adcin_batid_sw_en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_dig0_rsv0(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_chsel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_swctrl_en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_source_lbat_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_source_lbat2_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_start_extd(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_dac_extd(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_dac_extd_en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_pmu_thr_pdn_sw(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_pmu_thr_pdn_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_auxadc_pmu_thr_pdn_status(
	void);
extern unsigned int mt6355_upmu_set_auxadc_dig0_rsv1(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_start_shade_num(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_start_shade_en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_start_shade_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_adc_rdy_wakeup_clr(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_adc_rdy_fgadc_clr(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_adc_rdy_bat_plugin_clr(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_adc_rdy_pwron_clr(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_data_reuse_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_ch0_data_reuse_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_ch1_data_reuse_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_dcxo_data_reuse_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_data_reuse_en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_ch0_data_reuse_en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_ch1_data_reuse_en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_dcxo_data_reuse_en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_autorpt_prd(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_autorpt_en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_lbat_debt_max(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_lbat_debt_min(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_lbat_det_prd_15_0(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_lbat_det_prd_19_16(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_lbat_volt_max(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_lbat_irq_en_max(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_lbat_en_max(
	unsigned int val);
extern unsigned int mt6355_upmu_get_auxadc_lbat_max_irq_b(
	void);
extern unsigned int mt6355_upmu_set_auxadc_lbat_volt_min(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_lbat_irq_en_min(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_lbat_en_min(
	unsigned int val);
extern unsigned int mt6355_upmu_get_auxadc_lbat_min_irq_b(
	void);
extern unsigned int mt6355_upmu_get_auxadc_lbat_debounce_count_max(
	void);
extern unsigned int mt6355_upmu_get_auxadc_lbat_debounce_count_min(
	void);
extern unsigned int mt6355_upmu_set_auxadc_accdet_auto_spl(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_accdet_auto_rqst_clr(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_accdet_dig1_rsv0(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_accdet_dig0_rsv0(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_thr_debt_max(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_thr_debt_min(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_thr_det_prd_15_0(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_thr_det_prd_19_16(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_thr_volt_max(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_thr_irq_en_max(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_thr_en_max(
	unsigned int val);
extern unsigned int mt6355_upmu_get_auxadc_thr_max_irq_b(
	void);
extern unsigned int mt6355_upmu_set_auxadc_thr_volt_min(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_thr_irq_en_min(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_thr_en_min(
	unsigned int val);
extern unsigned int mt6355_upmu_get_auxadc_thr_min_irq_b(
	void);
extern unsigned int mt6355_upmu_get_auxadc_thr_debounce_count_max(
	void);
extern unsigned int mt6355_upmu_get_auxadc_thr_debounce_count_min(
	void);
extern unsigned int mt6355_upmu_set_efuse_gain_ch4_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_efuse_offset_ch4_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_efuse_gain_ch0_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_efuse_offset_ch0_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_efuse_gain_ch7_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_efuse_offset_ch7_trim(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_fgadc_start_sw(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_fgadc_start_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_fgadc_r_sw(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_fgadc_r_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_bat_plugin_start_sw(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_bat_plugin_start_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_dbg_dig0_rsv2(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_dbg_dig1_rsv2(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_impedance_cnt(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_impedance_chsel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_impedance_irq_clr(
	unsigned int val);
extern unsigned int mt6355_upmu_get_auxadc_impedance_irq_status(
	void);
extern unsigned int mt6355_upmu_set_auxadc_clr_imp_cnt_stop(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_impedance_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_imp_autorpt_prd(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_imp_autorpt_en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_bat_temp_froze_en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_bat_temp_debt_max(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_bat_temp_debt_min(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_bat_temp_det_prd_15_0(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_bat_temp_det_prd_19_16(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_bat_temp_volt_max(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_bat_temp_irq_en_max(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_bat_temp_en_max(
	unsigned int val);
extern unsigned int mt6355_upmu_get_auxadc_bat_temp_max_irq_b(
	void);
extern unsigned int mt6355_upmu_set_auxadc_bat_temp_volt_min(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_bat_temp_irq_en_min(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_bat_temp_en_min(
	unsigned int val);
extern unsigned int mt6355_upmu_get_auxadc_bat_temp_min_irq_b(
	void);
extern unsigned int mt6355_upmu_get_auxadc_bat_temp_debounce_count_max(
	void);
extern unsigned int mt6355_upmu_get_auxadc_bat_temp_debounce_count_min(
	void);
extern unsigned int mt6355_upmu_set_auxadc_lbat2_debt_max(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_lbat2_debt_min(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_lbat2_det_prd_15_0(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_lbat2_det_prd_19_16(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_lbat2_volt_max(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_lbat2_irq_en_max(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_lbat2_en_max(
	unsigned int val);
extern unsigned int mt6355_upmu_get_auxadc_lbat2_max_irq_b(
	void);
extern unsigned int mt6355_upmu_set_auxadc_lbat2_volt_min(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_lbat2_irq_en_min(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_lbat2_en_min(
	unsigned int val);
extern unsigned int mt6355_upmu_get_auxadc_lbat2_min_irq_b(
	void);
extern unsigned int mt6355_upmu_get_auxadc_lbat2_debounce_count_max(
	void);
extern unsigned int mt6355_upmu_get_auxadc_lbat2_debounce_count_min(
	void);
extern unsigned int mt6355_upmu_set_auxadc_mdbg_det_prd(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_mdbg_det_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_auxadc_mdbg_r_ptr(
	void);
extern unsigned int mt6355_upmu_get_auxadc_mdbg_w_ptr(
	void);
extern unsigned int mt6355_upmu_set_auxadc_mdbg_buf_length(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_mdrt_det_prd(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_mdrt_det_en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_mdrt_det_wkup_start_cnt(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_mdrt_det_wkup_start_clr(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_mdrt_det_wkup_start(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_mdrt_det_wkup_start_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_mdrt_det_wkup_en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_mdrt_det_srclken_ind(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_mdrt_det_rdy_st_prd(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_mdrt_det_rdy_st_en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_mdrt_det_start_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_jeita_irq_en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_jeita_en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_jeita_det_prd(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_jeita_debt(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_jeita_mipi_dis(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_jeita_froze_en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_jeita_volt_hot(
	unsigned int val);
extern unsigned int mt6355_upmu_get_auxadc_jeita_hot_irq(
	void);
extern unsigned int mt6355_upmu_set_auxadc_jeita_volt_warm(
	unsigned int val);
extern unsigned int mt6355_upmu_get_auxadc_jeita_warm_irq(
	void);
extern unsigned int mt6355_upmu_set_auxadc_jeita_volt_cool(
	unsigned int val);
extern unsigned int mt6355_upmu_get_auxadc_jeita_cool_irq(
	void);
extern unsigned int mt6355_upmu_set_auxadc_jeita_volt_cold(
	unsigned int val);
extern unsigned int mt6355_upmu_get_auxadc_jeita_cold_irq(
	void);
extern unsigned int mt6355_upmu_get_auxadc_jeita_debounce_count_cold(
	void);
extern unsigned int mt6355_upmu_get_auxadc_jeita_debounce_count_cool(
	void);
extern unsigned int mt6355_upmu_get_auxadc_jeita_debounce_count_warm(
	void);
extern unsigned int mt6355_upmu_get_auxadc_jeita_debounce_count_hot(
	void);
extern unsigned int mt6355_upmu_set_auxadc_dcxo_mdrt_det_prd(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_dcxo_mdrt_det_en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_dcxo_mdrt_det_wkup_start_cnt(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_dcxo_mdrt_det_wkup_start_clr(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_dcxo_mdrt_det_wkup_en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_dcxo_mdrt_det_wkup_start_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_dcxo_mdrt_det_wkup_start(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_dcxo_mdrt_det_srclken_ind(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_dcxo_ch4_mux_ap_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_nag_en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_nag_clr(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_nag_vbat1_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_nag_prd(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_nag_irq_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_auxadc_nag_c_dltv_irq(
	void);
extern unsigned int mt6355_upmu_set_auxadc_nag_zcv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_nag_c_dltv_th_15_0(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_nag_c_dltv_th_26_16(
	unsigned int val);
extern unsigned int mt6355_upmu_get_auxadc_nag_cnt_15_0(
	void);
extern unsigned int mt6355_upmu_get_auxadc_nag_cnt_25_16(
	void);
extern unsigned int mt6355_upmu_get_auxadc_nag_dltv(
	void);
extern unsigned int mt6355_upmu_get_auxadc_nag_c_dltv_15_0(
	void);
extern unsigned int mt6355_upmu_get_auxadc_nag_c_dltv_26_16(
	void);
extern unsigned int mt6355_upmu_set_auxadc_efuse_degc_cali(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_efuse_adc_cali_en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_efuse_1rsv0(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_efuse_o_vts(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_efuse_2rsv0(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_efuse_o_slope(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_efuse_o_slope_sign(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_efuse_3rsv0(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_efuse_auxadc_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_efuse_id(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_efuse_4rsv0(
	unsigned int val);
extern unsigned int mt6355_upmu_set_auxadc_rsv_1rsv0(
	unsigned int val);
extern unsigned int mt6355_upmu_get_da_adcin_vbat_en(
	void);
extern unsigned int mt6355_upmu_get_da_auxadc_vbat_en(
	void);
extern unsigned int mt6355_upmu_get_da_adcin_vsen_mux_en(
	void);
extern unsigned int mt6355_upmu_get_da_adcin_vsen_en(
	void);
extern unsigned int mt6355_upmu_get_da_adcin_chr_en(
	void);
extern unsigned int mt6355_upmu_get_da_baton_tdet_en(
	void);
extern unsigned int mt6355_upmu_get_da_adcin_batid_sw_en(
	void);
extern unsigned int mt6355_upmu_set_rg_auxadc_imp_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_auxadc_imp_ck_sw_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_auxadc_imp_ck_sw_en(
	void);
extern unsigned int mt6355_upmu_set_rg_auxadc_lbat_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_auxadc_lbat_ck_sw_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_auxadc_lbat_ck_sw_en(
	void);
extern unsigned int mt6355_upmu_set_rg_auxadc_thr_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_auxadc_thr_ck_sw_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_auxadc_thr_ck_sw_en(
	void);
extern unsigned int mt6355_upmu_set_rg_auxadc_bat_temp_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_auxadc_bat_temp_ck_sw_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_auxadc_bat_temp_ck_sw_en(
	void);
extern unsigned int mt6355_upmu_set_rg_auxadc_lbat2_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_auxadc_lbat2_ck_sw_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_auxadc_lbat2_ck_sw_en(
	void);
extern unsigned int mt6355_upmu_set_rg_auxadc_jeita_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_auxadc_jeita_ck_sw_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_auxadc_jeita_ck_sw_en(
	void);
extern unsigned int mt6355_upmu_set_rg_auxadc_nag_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_auxadc_nag_ck_sw_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_auxadc_nag_ck_sw_en(
	void);
extern unsigned int mt6355_upmu_set_rg_auxadc_new_priority_list_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_isink_trim_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_isink_trim_en(
	void);
extern unsigned int mt6355_upmu_set_rg_isink_trim_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_isink_trim_bias(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_isink_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_isink0_chop_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_isink0_chop_en(
	void);
extern unsigned int mt6355_upmu_set_rg_isink1_chop_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_isink1_chop_en(
	void);
extern unsigned int mt6355_upmu_set_rg_isink2_chop_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_isink2_chop_en(
	void);
extern unsigned int mt6355_upmu_set_rg_isink3_chop_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_isink3_chop_en(
	void);
extern unsigned int mt6355_upmu_set_rg_isink0_double(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_isink1_double(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_isink2_double(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_isink3_double(
	unsigned int val);
extern unsigned int mt6355_upmu_set_isink0_rsv1(
	unsigned int val);
extern unsigned int mt6355_upmu_set_isink0_rsv0(
	unsigned int val);
extern unsigned int mt6355_upmu_set_isink_ch0_step(
	unsigned int val);
extern unsigned int mt6355_upmu_set_isink1_rsv1(
	unsigned int val);
extern unsigned int mt6355_upmu_set_isink1_rsv0(
	unsigned int val);
extern unsigned int mt6355_upmu_set_isink_ch1_step(
	unsigned int val);
extern unsigned int mt6355_upmu_set_isink2_rsv1(
	unsigned int val);
extern unsigned int mt6355_upmu_set_isink2_rsv0(
	unsigned int val);
extern unsigned int mt6355_upmu_set_isink_ch2_step(
	unsigned int val);
extern unsigned int mt6355_upmu_set_isink3_rsv1(
	unsigned int val);
extern unsigned int mt6355_upmu_set_isink3_rsv0(
	unsigned int val);
extern unsigned int mt6355_upmu_set_isink_ch3_step(
	unsigned int val);
extern unsigned int mt6355_upmu_get_ad_ni_isink0_status(
	void);
extern unsigned int mt6355_upmu_get_ad_ni_isink1_status(
	void);
extern unsigned int mt6355_upmu_get_ad_ni_isink2_status(
	void);
extern unsigned int mt6355_upmu_get_ad_ni_isink3_status(
	void);
extern unsigned int mt6355_upmu_set_isink_ch3_en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_isink_ch2_en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_isink_ch1_en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_isink_ch0_en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_isink_chop3_en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_isink_chop2_en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_isink_chop1_en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_isink_chop0_en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_isink_ch3_bias_en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_isink_ch2_bias_en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_isink_ch1_bias_en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_isink_ch0_bias_en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_auddaclpwrup_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_auddacrpwrup_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_aud_dac_pwr_up_va32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_aud_dac_pwl_up_va32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audhplpwrup_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audhprpwrup_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audhplpwrup_ibias_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audhprpwrup_ibias_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audhplmuxinputsel_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audhprmuxinputsel_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audhplscdisable_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audhprscdisable_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audhplbsccurrent_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audhprbsccurrent_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audhploutpwrup_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audhproutpwrup_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audhploutauxpwrup_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audhproutauxpwrup_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_hplauxfbrsw_en_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_hplauxfbrsw_en_vaudp32(
	void);
extern unsigned int mt6355_upmu_set_rg_hprauxfbrsw_en_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_hprauxfbrsw_en_vaudp32(
	void);
extern unsigned int mt6355_upmu_set_rg_hplshort2hplaux_en_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_hplshort2hplaux_en_vaudp32(
	void);
extern unsigned int mt6355_upmu_set_rg_hprshort2hpraux_en_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_hprshort2hpraux_en_vaudp32(
	void);
extern unsigned int mt6355_upmu_set_rg_hploutstgctrl_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_hproutstgctrl_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_hploutputstbenh_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_hproutputstbenh_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audhpstartup_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audrefn_deres_en_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_audrefn_deres_en_vaudp32(
	void);
extern unsigned int mt6355_upmu_set_rg_hppshort2vcm_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audhpltrim_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audhprtrim_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audhplfinetrim_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audhprfinetrim_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audhptrim_en_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_audhptrim_en_vaudp32(
	void);
extern unsigned int mt6355_upmu_set_rg_hpinputstbenh_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_hpinputreset0_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_hpoutputreset0_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audhpdiffinpbiasadj_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audhplfcompressel_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audhphfcompressel_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audhphfcompbufgainsel_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audhpcomp_en_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_audhpcomp_en_vaudp32(
	void);
extern unsigned int mt6355_upmu_set_rg_audhpdecmgainadj_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audhpdedmgainadj_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audhspwrup_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audhspwrup_ibias_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audhsmuxinputsel_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audhsscdisable_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audhsbsccurrent_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audhsstartup_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_hsoutputstbenh_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_hsinputstbenh_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_hsinputreset0_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_hsoutputreset0_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_hsout_shortvcm_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audlolpwrup_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audlolpwrup_ibias_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audlolmuxinputsel_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audlolscdisable_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audlolbsccurrent_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audlostartup_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_loinputstbenh_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_looutputstbenh_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_loinputreset0_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_looutputreset0_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_loout_shortvcm_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audtrimbuf_inputmuxsel_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audtrimbuf_gainsel_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audtrimbuf_en_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_audtrimbuf_en_vaudp32(
	void);
extern unsigned int mt6355_upmu_set_rg_audhpspkdet_inputmuxsel_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audhpspkdet_outputmuxsel_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audhpspkdet_en_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_audhpspkdet_en_vaudp32(
	void);
extern unsigned int mt6355_upmu_set_rg_abidec_rsvd0_va32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_abidec_rsvd0_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_abidec_rsvd1_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_abidec_rsvd2_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audzcdmuxsel_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audzcdclksel_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audbiasadj_0_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audbiasadj_1_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audibiaspwrdn_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_rstb_decoder_va32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_sel_decoder_96k_va32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_sel_delay_vcore(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audglb_pwrdn_va32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audglb_lp_vow_en_va32(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_audglb_lp_vow_en_va32(
	void);
extern unsigned int mt6355_upmu_set_rg_audglb_lp2_vow_en_va32(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_audglb_lp2_vow_en_va32(
	void);
extern unsigned int mt6355_upmu_set_rg_lcldo_dec_en_va32(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_lcldo_dec_en_va32(
	void);
extern unsigned int mt6355_upmu_set_rg_lcldo_dec_pddis_en_va18(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_lcldo_dec_pddis_en_va18(
	void);
extern unsigned int mt6355_upmu_set_rg_lcldo_dec_remote_sense_va18(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_nvreg_en_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_nvreg_en_vaudp32(
	void);
extern unsigned int mt6355_upmu_set_rg_nvreg_pull0v_vaudp32(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audpmu_rsvd_va18(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audpreamplon(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audpreampldccen(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audpreampldcprecharge(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audpreamplpgatest(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audpreamplvscale(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audpreamplinputsel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audpreamplgain(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_bulkl_vcm_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_bulkl_vcm_en(
	void);
extern unsigned int mt6355_upmu_set_rg_audadclpwrup(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audadclinputsel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audpreampron(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audpreamprdccen(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audpreamprdcprecharge(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audpreamprpgatest(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audpreamprvscale(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audpreamprinputsel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audpreamprgain(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_bulkr_vcm_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_bulkr_vcm_en(
	void);
extern unsigned int mt6355_upmu_set_rg_audadcrpwrup(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audadcrinputsel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audulhalfbias(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audglbvowlpwen(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audpreamplpen(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audadc1ststagelpen(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audadc2ndstagelpen(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audadcflashlpen(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audpreampiddtest(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audadc1ststageiddtest(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audadc2ndstageiddtest(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audadcrefbufiddtest(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audadcflashiddtest(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audadcclkrstb(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audadcclksel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audadcclksource(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audadcclkgenmode(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audadcdac0p25fs(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audpreampaafen(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_dccvcmbuflpmodsel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_dccvcmbuflpswen(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audsparepga(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audadc1ststagesdenb(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audadc2ndstagereset(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audadc3rdstagereset(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audadcfsreset(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audadcwidecm(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audadcnopatest(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audadcbypass(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audadcffbypass(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audadcdacfbcurrent(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audadcdaciddtest(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audadcdacnrz(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audadcnodem(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audadcdactest(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audadctestdata(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audrctunel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audrctunelsel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audrctuner(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audrctunersel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rgs_audrctunelread(
	void);
extern unsigned int mt6355_upmu_get_rgs_audrctunerread(
	void);
extern unsigned int mt6355_upmu_set_rg_audspareva30(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audspareva18(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_auddigmicen(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_auddigmicbias(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_dmichpclken(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_auddigmicpduty(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_auddigmicnduty(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_dmicmonen(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_dmicmonsel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audsparevmic(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audpwdbmicbias0(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audmicbias0dcsw0p1en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audmicbias0dcsw0p2en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audmicbias0dcsw0nen(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audmicbias0vref(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audmicbias0lowpen(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audpwdbmicbias2(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audmicbias0dcsw2p1en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audmicbias0dcsw2p2en(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audmicbias0dcsw2nen(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audpwdbmicbias1(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audmicbias1dcsw1pen(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audmicbias1dcsw1nen(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audmicbias1vref(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audmicbias1lowpen(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audmicbias1hven(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audmicbias1hvvref(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_bandgapgen(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audaccdetmicbias0pulllow(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audaccdetmicbias1pulllow(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audaccdetmicbias2pulllow(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audaccdetvin1pulllow(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_einthirenb(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audaccdetvthacal(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audaccdetvthbcal(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audaccdettvdet(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_accdetsel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_swbufmodsel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_swbufswen(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_eintcompvth(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_eintconfigaccdet(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_accdetspareva30(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audencspareva30(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audencspareva18(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_pll_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_pll_en(
	void);
extern unsigned int mt6355_upmu_set_rg_pllbs_rst(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_pll_dcko_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_pll_div1(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_pll_rlatch_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_pll_rlatch_en(
	void);
extern unsigned int mt6355_upmu_set_rg_pll_pdiv1_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_pll_pdiv1_en(
	void);
extern unsigned int mt6355_upmu_set_rg_pll_pdiv1(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_pll_bc(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_pll_bp(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_pll_br(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_cko_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_pll_ibsel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_pll_ckt_sel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_pll_vct_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_pll_vct_en(
	void);
extern unsigned int mt6355_upmu_set_rg_pll_ckt_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_pll_ckt_en(
	void);
extern unsigned int mt6355_upmu_set_rg_pll_hpm_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_pll_hpm_en(
	void);
extern unsigned int mt6355_upmu_set_rg_pll_dchp_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_pll_dchp_en(
	void);
extern unsigned int mt6355_upmu_set_rg_pll_cdiv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcoband(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_ckdrv_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ckdrv_en(
	void);
extern unsigned int mt6355_upmu_set_rg_pll_dchp_aen(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_pll_rsva(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audzcdenable(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audzcdgainsteptime(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audzcdgainstepsize(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audzcdtimeoutmodesel(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audzcdclksel_vaudp15(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audzcdmuxsel_vaudp15(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audlolgain(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audlorgain(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audhplgain(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audhprgain(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audhsgain(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audivlgain(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_audivrgain(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_audintgain1(
	void);
extern unsigned int mt6355_upmu_get_rg_audintgain2(
	void);
extern unsigned int mt6355_upmu_set_rsv_con0_rsv(
	unsigned int val);
extern unsigned int mt6355_upmu_set_rg_vcn33_bt_vosel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vcn33_bt_vosel(
	void);
extern unsigned int mt6355_upmu_set_rg_vcn33_wifi_vosel(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_vcn33_wifi_vosel(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcn33_bt_dummy_load(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcn33_bt_dummy_load(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcn33_wifi_dummy_load(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcn33_wifi_dummy_load(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcn33_bt_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcn33_bt_hw0_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcn33_wifi_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcn33_wifi_hw0_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcn33_bt_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcn33_bt_hw1_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcn33_wifi_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcn33_wifi_hw1_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcn33_bt_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcn33_bt_hw2_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcn33_wifi_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcn33_wifi_hw2_op_cfg(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcn33_bt_hw0_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcn33_bt_hw0_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcn33_wifi_hw0_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcn33_wifi_hw0_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcn33_bt_hw1_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcn33_bt_hw1_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcn33_wifi_hw1_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcn33_wifi_hw1_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcn33_bt_hw2_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcn33_bt_hw2_op_en(
	void);
extern unsigned int mt6355_upmu_set_rg_ldo_vcn33_wifi_hw2_op_en(
	unsigned int val);
extern unsigned int mt6355_upmu_get_rg_ldo_vcn33_wifi_hw2_op_en(
	void);
extern unsigned int mt6355_upmu_get_da_vcn33_bt_en(
	void);
extern unsigned int mt6355_upmu_get_da_vcn33_wifi_en(
	void);
extern unsigned int mt6355_upmu_get_da_vcn33_bt_dummy_load(
	void);
extern unsigned int mt6355_upmu_get_da_vcn33_wifi_dummy_load(
	void);
extern unsigned int mt6355_upmu_get_da_vcn33_bt_stb(
	void);
extern unsigned int mt6355_upmu_get_da_vcn33_wifi_stb(
	void);
extern unsigned int mt6355_upmu_get_da_vcn33_bt_mode(
	void);
extern unsigned int mt6355_upmu_get_da_vcn33_wifi_mode(
	void);
#endif		/* _MT_PMIC_API_H_ */
