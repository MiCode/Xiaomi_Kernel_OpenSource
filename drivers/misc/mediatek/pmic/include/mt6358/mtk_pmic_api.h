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

#ifndef _MT_PMIC_API_H_
#define _MT_PMIC_API_H_

extern unsigned int mt6358_upmu_get_top0_ana_id(
	void);
extern unsigned int mt6358_upmu_get_top0_dig_id(
	void);
extern unsigned int mt6358_upmu_get_top0_ana_minor_rev(
	void);
extern unsigned int mt6358_upmu_get_top0_ana_major_rev(
	void);
extern unsigned int mt6358_upmu_get_top0_dig_minor_rev(
	void);
extern unsigned int mt6358_upmu_get_top0_dig_major_rev(
	void);
extern unsigned int mt6358_upmu_get_top0_dsn_cbs(
	void);
extern unsigned int mt6358_upmu_get_top0_dsn_bix(
	void);
extern unsigned int mt6358_upmu_get_top0_dsn_esp(
	void);
extern unsigned int mt6358_upmu_get_top0_dsn_fpi(
	void);
extern unsigned int mt6358_upmu_get_hwcid(
	void);
extern unsigned int mt6358_upmu_get_swcid(
	void);
extern unsigned int mt6358_upmu_get_sts_pwrkey(
	void);
extern unsigned int mt6358_upmu_get_sts_rtca(
	void);
extern unsigned int mt6358_upmu_get_sts_chrin(
	void);
extern unsigned int mt6358_upmu_get_sts_spar(
	void);
extern unsigned int mt6358_upmu_get_sts_rboot(
	void);
extern unsigned int mt6358_upmu_get_sts_uvlo(
	void);
extern unsigned int mt6358_upmu_get_sts_pgfail(
	void);
extern unsigned int mt6358_upmu_get_sts_psoc(
	void);
extern unsigned int mt6358_upmu_get_sts_thrdn(
	void);
extern unsigned int mt6358_upmu_get_sts_wrst(
	void);
extern unsigned int mt6358_upmu_get_sts_crst(
	void);
extern unsigned int mt6358_upmu_get_sts_pkeylp(
	void);
extern unsigned int mt6358_upmu_get_sts_normoff(
	void);
extern unsigned int mt6358_upmu_get_sts_bwdt(
	void);
extern unsigned int mt6358_upmu_get_sts_ddlo(
	void);
extern unsigned int mt6358_upmu_get_sts_wdt(
	void);
extern unsigned int mt6358_upmu_get_sts_pupsrc(
	void);
extern unsigned int mt6358_upmu_get_sts_keypwr(
	void);
extern unsigned int mt6358_upmu_get_sts_pksp(
	void);
extern unsigned int mt6358_upmu_set_rg_poffsts_clr(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ponsts_clr(
	unsigned int val);
extern unsigned int mt6358_upmu_get_vio18_pg_deb(
	void);
extern unsigned int mt6358_upmu_get_vsram_proc12_pg_deb(
	void);
extern unsigned int mt6358_upmu_get_vsram_proc11_pg_deb(
	void);
extern unsigned int mt6358_upmu_get_va12_pg_deb(
	void);
extern unsigned int mt6358_upmu_get_vsram_gpu_pg_deb(
	void);
extern unsigned int mt6358_upmu_get_vsram_others_pg_deb(
	void);
extern unsigned int mt6358_upmu_get_vaux18_pg_deb(
	void);
extern unsigned int mt6358_upmu_get_vxo22_pg_deb(
	void);
extern unsigned int mt6358_upmu_get_vdram1_pg_deb(
	void);
extern unsigned int mt6358_upmu_get_vproc12_pg_deb(
	void);
extern unsigned int mt6358_upmu_get_vproc11_pg_deb(
	void);
extern unsigned int mt6358_upmu_get_vs1_pg_deb(
	void);
extern unsigned int mt6358_upmu_get_vgpu_pg_deb(
	void);
extern unsigned int mt6358_upmu_get_vmodem_pg_deb(
	void);
extern unsigned int mt6358_upmu_get_vcore_pg_deb(
	void);
extern unsigned int mt6358_upmu_get_vs2_pg_deb(
	void);
extern unsigned int mt6358_upmu_get_rsv_pg_deb(
	void);
extern unsigned int mt6358_upmu_get_vaud28_pg_deb(
	void);
extern unsigned int mt6358_upmu_get_vusb_pg_deb(
	void);
extern unsigned int mt6358_upmu_get_vdram2_pg_deb(
	void);
extern unsigned int mt6358_upmu_get_vio28_pg_deb(
	void);
extern unsigned int mt6358_upmu_get_vemc_pg_deb(
	void);
extern unsigned int mt6358_upmu_get_strup_vio18_pg_status(
	void);
extern unsigned int mt6358_upmu_get_strup_vsram_proc12_pg_status(
	void);
extern unsigned int mt6358_upmu_get_strup_vsram_proc11_pg_status(
	void);
extern unsigned int mt6358_upmu_get_strup_va12_pg_status(
	void);
extern unsigned int mt6358_upmu_get_strup_vsram_gpu_pg_status(
	void);
extern unsigned int mt6358_upmu_get_strup_vsram_others_pg_status(
	void);
extern unsigned int mt6358_upmu_get_strup_vaux18_pg_status(
	void);
extern unsigned int mt6358_upmu_get_strup_vxo22_pg_status(
	void);
extern unsigned int mt6358_upmu_get_strup_vdram1_pg_status(
	void);
extern unsigned int mt6358_upmu_get_strup_vproc12_pg_status(
	void);
extern unsigned int mt6358_upmu_get_strup_vproc11_pg_status(
	void);
extern unsigned int mt6358_upmu_get_strup_vs1_pg_status(
	void);
extern unsigned int mt6358_upmu_get_strup_vgpu_pg_status(
	void);
extern unsigned int mt6358_upmu_get_strup_vmodem_pg_status(
	void);
extern unsigned int mt6358_upmu_get_strup_vcore_pg_status(
	void);
extern unsigned int mt6358_upmu_get_strup_vs2_pg_status(
	void);
extern unsigned int mt6358_upmu_get_strup_rsv_pg_status(
	void);
extern unsigned int mt6358_upmu_get_strup_vaud28_pg_status(
	void);
extern unsigned int mt6358_upmu_get_strup_vusb_pg_status(
	void);
extern unsigned int mt6358_upmu_get_strup_vdram2_pg_status(
	void);
extern unsigned int mt6358_upmu_get_strup_vio28_pg_status(
	void);
extern unsigned int mt6358_upmu_get_strup_vemc_pg_status(
	void);
extern unsigned int mt6358_upmu_get_strup_vdram1_oc_status(
	void);
extern unsigned int mt6358_upmu_get_strup_vproc12_oc_status(
	void);
extern unsigned int mt6358_upmu_get_strup_vproc11_oc_status(
	void);
extern unsigned int mt6358_upmu_get_strup_vs1_oc_status(
	void);
extern unsigned int mt6358_upmu_get_strup_vgpu_oc_status(
	void);
extern unsigned int mt6358_upmu_get_strup_vmodem_oc_status(
	void);
extern unsigned int mt6358_upmu_get_strup_vcore_oc_status(
	void);
extern unsigned int mt6358_upmu_get_strup_vs2_oc_status(
	void);
extern unsigned int mt6358_upmu_get_pmu_thermal_deb(
	void);
extern unsigned int mt6358_upmu_get_strup_thermal_status(
	void);
extern unsigned int mt6358_upmu_set_rg_srclken_in0_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_srclken_in0_en(
	void);
extern unsigned int mt6358_upmu_set_rg_srclken_in0_hw_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_srclken_in1_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_srclken_in1_en(
	void);
extern unsigned int mt6358_upmu_set_rg_srclken_in1_hw_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_srclken_in_sync_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_srclken_in_sync_en(
	void);
extern unsigned int mt6358_upmu_set_rg_osc_en_auto_off(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_osc_en_auto_off(
	void);
extern unsigned int mt6358_upmu_get_test_out(
	void);
extern unsigned int mt6358_upmu_set_rg_mon_flag_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_mon_grp_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_nandtree_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_test_auxadc(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_efuse_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_test_strup(
	unsigned int val);
extern unsigned int mt6358_upmu_set_testmode_sw(
	unsigned int val);
extern unsigned int mt6358_upmu_get_pmu_test_mode_scan(
	void);
extern unsigned int mt6358_upmu_get_pwrkey_deb(
	void);
extern unsigned int mt6358_upmu_get_chrdet_deb(
	void);
extern unsigned int mt6358_upmu_get_homekey_deb(
	void);
extern unsigned int mt6358_upmu_set_rg_pmu_tdsel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_spi_tdsel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_aud_tdsel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_e32cal_tdsel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_pmu_rdsel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_spi_rdsel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_aud_rdsel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_e32cal_rdsel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_smt_wdtrstb_in(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_smt_srclken_in0(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_smt_srclken_in1(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_smt_rtc_32k1v8_0(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_smt_rtc_32k1v8_1(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_smt_homekey(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_smt_scp_vreq_vao(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_smt_sd_card_det_n(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_smt_spi_clk(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_smt_spi_csn(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_smt_spi_mosi(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_smt_spi_miso(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_smt_aud_clk_mosi(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_smt_aud_dat_mosi0(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_smt_aud_dat_mosi1(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_smt_aud_sync_mosi(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_smt_aud_clk_miso(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_smt_aud_dat_miso0(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_smt_aud_dat_miso1(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_smt_aud_sync_miso(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_top_rsv0(
	void);
extern unsigned int mt6358_upmu_get_rg_top_rsv1(
	void);
extern unsigned int mt6358_upmu_set_rg_octl_srclken_in0(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_octl_srclken_in1(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_octl_rtc_32k1v8_0(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_octl_rtc_32k1v8_1(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_octl_spi_clk(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_octl_spi_csn(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_octl_spi_mosi(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_octl_spi_miso(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_octl_aud_clk_mosi(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_octl_aud_dat_mosi0(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_octl_aud_dat_mosi1(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_octl_aud_sync_mosi(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_octl_aud_clk_miso(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_octl_aud_dat_miso0(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_octl_aud_dat_miso1(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_octl_aud_sync_miso(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_octl_homekey(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_octl_scp_vreq_vao(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_octl_sd_card_det_n(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_srclken_in0_filter_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_srclken_in0_filter_en(
	void);
extern unsigned int mt6358_upmu_set_rg_srclken_in1_filter_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_srclken_in1_filter_en(
	void);
extern unsigned int mt6358_upmu_set_rg_rtc32k_1v8_0_filter_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_rtc32k_1v8_0_filter_en(
	void);
extern unsigned int mt6358_upmu_set_rg_rtc32k_1v8_1_filter_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_rtc32k_1v8_1_filter_en(
	void);
extern unsigned int mt6358_upmu_set_rg_spi_clk_filter_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_spi_clk_filter_en(
	void);
extern unsigned int mt6358_upmu_set_rg_spi_csn_filter_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_spi_csn_filter_en(
	void);
extern unsigned int mt6358_upmu_set_rg_spi_mosi_filter_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_spi_mosi_filter_en(
	void);
extern unsigned int mt6358_upmu_set_rg_spi_miso_filter_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_spi_miso_filter_en(
	void);
extern unsigned int mt6358_upmu_set_rg_aud_clk_mosi_filter_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_aud_clk_mosi_filter_en(
	void);
extern unsigned int mt6358_upmu_set_rg_aud_dat_mosi0_filter_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_aud_dat_mosi0_filter_en(
	void);
extern unsigned int mt6358_upmu_set_rg_aud_dat_mosi1_filter_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_aud_dat_mosi1_filter_en(
	void);
extern unsigned int mt6358_upmu_set_rg_aud_sync_mosi_filter_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_aud_sync_mosi_filter_en(
	void);
extern unsigned int mt6358_upmu_set_rg_aud_clk_miso_filter_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_aud_clk_miso_filter_en(
	void);
extern unsigned int mt6358_upmu_set_rg_aud_dat_miso0_filter_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_aud_dat_miso0_filter_en(
	void);
extern unsigned int mt6358_upmu_set_rg_aud_dat_miso1_filter_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_aud_dat_miso1_filter_en(
	void);
extern unsigned int mt6358_upmu_set_rg_aud_sync_miso_filter_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_aud_sync_miso_filter_en(
	void);
extern unsigned int mt6358_upmu_set_rg_wdtrstb_in_filter_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_wdtrstb_in_filter_en(
	void);
extern unsigned int mt6358_upmu_set_rg_homekey_filter_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_homekey_filter_en(
	void);
extern unsigned int mt6358_upmu_set_rg_scp_vreq_vao_filter_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_scp_vreq_vao_filter_en(
	void);
extern unsigned int mt6358_upmu_set_rg_sd_card_det_n_filter_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_sd_card_det_n_filter_en(
	void);
extern unsigned int mt6358_upmu_set_rg_srclken_in0_rcsel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_srclken_in1_rcsel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_rtc32k_1v8_0_rcsel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_rtc32k_1v8_1_rcsel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_spi_clk_rcsel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_spi_csn_rcsel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_spi_mosi_rcsel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_spi_miso_rcsel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_aud_clk_mosi_rcsel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_aud_dat_mosi0_rcsel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_aud_dat_mosi1_rcsel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_aud_sync_mosi_rcsel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_aud_clk_miso_rcsel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_aud_dat_miso0_rcsel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_aud_dat_miso1_rcsel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_aud_sync_miso_rcsel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_wdtrstb_in_rcsel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_homekey_rcsel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_scp_vreq_vao_rcsel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_sd_card_det_n_rcsel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_top_status(
	unsigned int val);
extern unsigned int mt6358_upmu_get_vm_mode(
	void);
extern unsigned int mt6358_upmu_get_top1_ana_id(
	void);
extern unsigned int mt6358_upmu_get_top1_dig_id(
	void);
extern unsigned int mt6358_upmu_get_top1_ana_minor_rev(
	void);
extern unsigned int mt6358_upmu_get_top1_ana_major_rev(
	void);
extern unsigned int mt6358_upmu_get_top1_dig_minor_rev(
	void);
extern unsigned int mt6358_upmu_get_top1_dig_major_rev(
	void);
extern unsigned int mt6358_upmu_get_top1_dsn_cbs(
	void);
extern unsigned int mt6358_upmu_get_top1_dsn_bix(
	void);
extern unsigned int mt6358_upmu_get_top1_dsn_esp(
	void);
extern unsigned int mt6358_upmu_get_top1_dsn_fpi(
	void);
extern unsigned int mt6358_upmu_set_gpio_dir0(
	unsigned int val);
extern unsigned int mt6358_upmu_set_gpio_dir1(
	unsigned int val);
extern unsigned int mt6358_upmu_set_gpio_pullen0(
	unsigned int val);
extern unsigned int mt6358_upmu_set_gpio_pullen1(
	unsigned int val);
extern unsigned int mt6358_upmu_set_gpio_pullsel0(
	unsigned int val);
extern unsigned int mt6358_upmu_set_gpio_pullsel1(
	unsigned int val);
extern unsigned int mt6358_upmu_set_gpio_dinv0(
	unsigned int val);
extern unsigned int mt6358_upmu_set_gpio_dinv1(
	unsigned int val);
extern unsigned int mt6358_upmu_set_gpio_dout0(
	unsigned int val);
extern unsigned int mt6358_upmu_set_gpio_dout1(
	unsigned int val);
extern unsigned int mt6358_upmu_get_gpio_pi0(
	void);
extern unsigned int mt6358_upmu_get_gpio_pi1(
	void);
extern unsigned int mt6358_upmu_get_gpio_poe0(
	void);
extern unsigned int mt6358_upmu_get_gpio_poe1(
	void);
extern unsigned int mt6358_upmu_set_gpio0_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_gpio1_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_gpio2_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_gpio3_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_gpio4_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_gpio5_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_gpio6_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_gpio7_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_gpio8_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_gpio9_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_gpio10_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_gpio11_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_gpio12_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_gpio13_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_gpio14_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_gpio15_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_gpio16_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_gpio17_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_gpio18_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_gpio_rsv(
	unsigned int val);
extern unsigned int mt6358_upmu_get_top2_ana_id(
	void);
extern unsigned int mt6358_upmu_get_top2_dig_id(
	void);
extern unsigned int mt6358_upmu_get_top2_ana_minor_rev(
	void);
extern unsigned int mt6358_upmu_get_top2_ana_major_rev(
	void);
extern unsigned int mt6358_upmu_get_top2_dig_minor_rev(
	void);
extern unsigned int mt6358_upmu_get_top2_dig_major_rev(
	void);
extern unsigned int mt6358_upmu_get_top2_dsn_cbs(
	void);
extern unsigned int mt6358_upmu_get_top2_dsn_bix(
	void);
extern unsigned int mt6358_upmu_get_top2_dsn_esp(
	void);
extern unsigned int mt6358_upmu_get_top2_dsn_fpi(
	void);
extern unsigned int mt6358_upmu_set_rg_g_smps_ck_pdn(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_g_smps_test_ck_pdn(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_intrp_ck_pdn(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_intrp_pre_oc_ck_pdn(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_efuse_ck_pdn(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_eint_32k_ck_pdn(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_pmu1m_ck_pdn(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_spi_ck_pdn(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_reg_ck_pdn(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_pmu32k_ck_pdn(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_fqmtr_32k_ck_pdn(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_fqmtr_ck_pdn(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_pmu26m_ck_pdn(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_pmu128k_ck_pdn(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_rtc26m_ck_pdn(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_rtc32k_ck_pdn(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_rtc32k_1v8_0_pdn(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_rtc32k_1v8_1_pdn(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_trim_128k_ck_pdn(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_bgr_test_ck_pdn(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_pchr_test_ck_pdn(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_fqmtr_ck_cksel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_rtc_32k1v8_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_bgr_test_ck_cksel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_pchr_test_ck_cksel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_26m_ck_sel_hwen(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_26m_ck_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_pmu_1m_ck_sel_hwen(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_pmu_1m_ck_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_pmu32k_ck_cksel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_top_cksel_con0_rsv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_srcvolten_sw(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vowen_sw(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vow_ldo_vsram_core_dvs_done(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vow_buck_vcore_dvs_done(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_srcvolten_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vowen_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vow_ldo_vsram_core_dvs_sw_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vow_buck_vcore_dvs_sw_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_top_cksel_con2_rsv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_reg_ck_divsel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_top_ckdivsel_con0_rsv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_g_smps_ck_pdn_hwen(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_reg_ck_pdn_hwen(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_efuse_ck_pdn_hwen(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_eint_32k_ck_pdn_hwen(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_rtc26m_ck_pdn_hwen(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_pmu26m_ck_pdn_hwen(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_pmu_vxo22_on(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_pmu_vxo22_on_sw_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_pmu_vxo22_on_sw_en(
	void);
extern unsigned int mt6358_upmu_set_top_ckhwen_con0_rsv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_pmu128k_ck_tst_dis(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_smps_ck_tst_dis(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_xo_clk_26m_pmu_tst_dis(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_xo_clk_26m_dig_tst_dis(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_rtc_26m_ck_tst_dis(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_rtc_32k_ck_tst_dis(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_pmu_m_ck_tst_dis(
	unsigned int val);
extern unsigned int mt6358_upmu_set_top_cktst_con0_rsv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_pmu128k_ck_tstsel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_smps_ck_tstsel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_xo_clk_26m_pmu_tstsel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_xo_clk_26m_dig_tstsel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_rtc_26m_ck_tstsel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_rtc_32k_ck_tstsel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_pmu_m_ck_tstsel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_efuse_ck_tstsel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_bgr_test_ck_tstsel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_pchr_test_ck_tstsel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_fqmtr_ck_tstsel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_osc_sel_sw_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_osc_sel_sw_en(
	void);
extern unsigned int mt6358_upmu_set_rg_osc_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_osc_en_sw_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_osc_en_sw_en(
	void);
extern unsigned int mt6358_upmu_set_rg_osc_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_osc_en(
	void);
extern unsigned int mt6358_upmu_set_rg_srcvolten_lp_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_srcvolten_lp_en(
	void);
extern unsigned int mt6358_upmu_set_rg_srclken0_lp_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_srclken0_lp_en(
	void);
extern unsigned int mt6358_upmu_set_rg_srclken1_lp_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_srclken1_lp_en(
	void);
extern unsigned int mt6358_upmu_set_rg_srclken2_lp_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_srclken2_lp_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_lp_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_lp_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_lp_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_lp_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_pfm_flag(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_pfm_flag_sw_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_pfm_flag_sw_en(
	void);
extern unsigned int mt6358_upmu_set_rg_dcxo26m_rdy(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_dcxo26m_rdy_sw_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_dcxo26m_rdy_sw_en(
	void);
extern unsigned int mt6358_upmu_set_rg_pmu_lp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_pmu_lp_sw_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_pmu_lp_sw_en(
	void);
extern unsigned int mt6358_upmu_set_rg_pmu_mdb_dcm_sw_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_pmu_mdb_dcm_sw_en(
	void);
extern unsigned int mt6358_upmu_set_rg_pmu_mdb_dcm_sw_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_get_ro_handover_debug(
	void);
extern unsigned int mt6358_upmu_set_rg_efuse_man_rst(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_driver_rst(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_fqmtr_rst(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_rtc_rst(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_type_c_cc_rst(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_clk_trim_rst(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_srclken_rst(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_prot_pmpp_rst(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_spk_rst(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ft_vr_sysrstb(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_cali_rst(
	unsigned int val);
extern unsigned int mt6358_upmu_set_top_rst_con1_rsv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_chr_ldo_det_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_chr_ldo_det_sw(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_chrwdt_flag_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_chrwdt_flag_sw(
	unsigned int val);
extern unsigned int mt6358_upmu_set_top_rst_con2_rsv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_wdtrstb_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_wdtrstb_en(
	void);
extern unsigned int mt6358_upmu_set_rg_wdtrstb_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_get_wdtrstb_status(
	void);
extern unsigned int mt6358_upmu_set_wdtrstb_status_clr(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_wdtrstb_fb_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_wdtrstb_fb_en(
	void);
extern unsigned int mt6358_upmu_set_rg_wdtrstb_deb(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_homekey_rst_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_homekey_rst_en(
	void);
extern unsigned int mt6358_upmu_set_rg_pwrkey_rst_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_pwrkey_rst_en(
	void);
extern unsigned int mt6358_upmu_set_rg_pwrrst_tmr_dis(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_pwrkey_rst_td(
	unsigned int val);
extern unsigned int mt6358_upmu_set_top_rst_misc_rsv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_vpwrin_rstb_status(
	unsigned int val);
extern unsigned int mt6358_upmu_set_ddlo_rstb_status(
	unsigned int val);
extern unsigned int mt6358_upmu_set_uvlo_rstb_status(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rtc_ddlo_rstb_status(
	unsigned int val);
extern unsigned int mt6358_upmu_set_chrwdt_reg_rstb_status(
	unsigned int val);
extern unsigned int mt6358_upmu_set_chrdet_reg_rstb_status(
	unsigned int val);
extern unsigned int mt6358_upmu_set_bwdt_ddlo_rstb_status(
	unsigned int val);
extern unsigned int mt6358_upmu_set_top_rst_status_rsv(
	unsigned int val);
extern unsigned int mt6358_upmu_get_top2_elr_len(
	void);
extern unsigned int mt6358_upmu_set_rg_top2_rsv0(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_top2_rsv1(
	unsigned int val);
extern unsigned int mt6358_upmu_get_top3_ana_id(
	void);
extern unsigned int mt6358_upmu_get_top3_dig_id(
	void);
extern unsigned int mt6358_upmu_get_top3_ana_minor_rev(
	void);
extern unsigned int mt6358_upmu_get_top3_ana_major_rev(
	void);
extern unsigned int mt6358_upmu_get_top3_dig_minor_rev(
	void);
extern unsigned int mt6358_upmu_get_top3_dig_major_rev(
	void);
extern unsigned int mt6358_upmu_get_top3_dsn_cbs(
	void);
extern unsigned int mt6358_upmu_get_top3_dsn_bix(
	void);
extern unsigned int mt6358_upmu_get_top3_dsn_esp(
	void);
extern unsigned int mt6358_upmu_get_top3_dsn_fpi(
	void);
extern unsigned int mt6358_upmu_set_rg_int_en_spi_cmd_alert(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_spi_cmd_alert(
	void);
extern unsigned int mt6358_upmu_set_rg_int_mask_spi_cmd_alert(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_spi_cmd_alert(
	void);
extern unsigned int mt6358_upmu_set_rg_int_mask_buck_top(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_ldo_top(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_psc_top(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_sck_top(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_bm_top(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_hk_top(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_xpp_top(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_aud_top(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_misc_top(
	unsigned int val);
extern unsigned int mt6358_upmu_get_int_status_buck_top(
	void);
extern unsigned int mt6358_upmu_get_int_status_ldo_top(
	void);
extern unsigned int mt6358_upmu_get_int_status_psc_top(
	void);
extern unsigned int mt6358_upmu_get_int_status_sck_top(
	void);
extern unsigned int mt6358_upmu_get_int_status_bm_top(
	void);
extern unsigned int mt6358_upmu_get_int_status_hk_top(
	void);
extern unsigned int mt6358_upmu_get_int_status_xpp_top(
	void);
extern unsigned int mt6358_upmu_get_int_status_aud_top(
	void);
extern unsigned int mt6358_upmu_get_int_status_misc_top(
	void);
extern unsigned int mt6358_upmu_get_int_status_top_rsv(
	void);
extern unsigned int mt6358_upmu_get_int_raw_status_buck_top(
	void);
extern unsigned int mt6358_upmu_get_int_raw_status_ldo_top(
	void);
extern unsigned int mt6358_upmu_get_int_raw_status_psc_top(
	void);
extern unsigned int mt6358_upmu_get_int_raw_status_sck_top(
	void);
extern unsigned int mt6358_upmu_get_int_raw_status_bm_top(
	void);
extern unsigned int mt6358_upmu_get_int_raw_status_hk_top(
	void);
extern unsigned int mt6358_upmu_get_int_raw_status_xpp_top(
	void);
extern unsigned int mt6358_upmu_get_int_raw_status_aud_top(
	void);
extern unsigned int mt6358_upmu_get_int_raw_status_misc_top(
	void);
extern unsigned int mt6358_upmu_get_int_raw_status_top_rsv(
	void);
extern unsigned int mt6358_upmu_set_rg_int_polarity(
	unsigned int val);
extern unsigned int mt6358_upmu_get_plt0_ana_id(
	void);
extern unsigned int mt6358_upmu_get_plt0_dig_id(
	void);
extern unsigned int mt6358_upmu_get_plt0_ana_minor_rev(
	void);
extern unsigned int mt6358_upmu_get_plt0_ana_major_rev(
	void);
extern unsigned int mt6358_upmu_get_plt0_dig_minor_rev(
	void);
extern unsigned int mt6358_upmu_get_plt0_dig_major_rev(
	void);
extern unsigned int mt6358_upmu_get_plt0_dsn_cbs(
	void);
extern unsigned int mt6358_upmu_get_plt0_dsn_bix(
	void);
extern unsigned int mt6358_upmu_get_plt0_dsn_fpi(
	void);
extern unsigned int mt6358_upmu_set_rg_osc_128k_trim_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_osc_128k_trim_en(
	void);
extern unsigned int mt6358_upmu_set_rg_osc_128k_trim_rate(
	unsigned int val);
extern unsigned int mt6358_upmu_get_da_osc_128k_trim(
	void);
extern unsigned int mt6358_upmu_set_rg_otp_pa(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_otp_pdin(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_otp_ptm(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_otp_pwe(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_otp_pprog(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_otp_pwe_src(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_otp_prog_pkey(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_otp_rd_pkey(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_otp_rd_trig(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_rd_rdy_bypass(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_skip_otp_out(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_otp_rd_sw(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_otp_dout_sw(
	void);
extern unsigned int mt6358_upmu_get_rg_otp_rd_busy(
	void);
extern unsigned int mt6358_upmu_get_rg_otp_rd_ack(
	void);
extern unsigned int mt6358_upmu_get_rg_otp_pa_sw(
	void);
extern unsigned int mt6358_upmu_set_tma_key(
	unsigned int val);
extern unsigned int mt6358_upmu_set_top_mdb_rsv0(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_mdb_dm1_ds_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_mdb_dm1_ds_en(
	void);
extern unsigned int mt6358_upmu_set_top_mdb_rsv1(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_pmu_mdb_bridge_bypass_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_pmu_mdb_bridge_bypass_en(
	void);
extern unsigned int mt6358_upmu_set_rg_osc_128k_trim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_efuse_osc_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_get_spislv_ana_id(
	void);
extern unsigned int mt6358_upmu_get_spislv_dig_id(
	void);
extern unsigned int mt6358_upmu_get_spislv_ana_minor_rev(
	void);
extern unsigned int mt6358_upmu_get_spislv_ana_major_rev(
	void);
extern unsigned int mt6358_upmu_get_spislv_dig_minor_rev(
	void);
extern unsigned int mt6358_upmu_get_spislv_dig_major_rev(
	void);
extern unsigned int mt6358_upmu_get_spislv_dsn_cbs(
	void);
extern unsigned int mt6358_upmu_get_spislv_dsn_bix(
	void);
extern unsigned int mt6358_upmu_get_spislv_dsn_esp(
	void);
extern unsigned int mt6358_upmu_get_spislv_dsn_fpi(
	void);
extern unsigned int mt6358_upmu_set_rg_slp_rw_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_slp_rw_en(
	void);
extern unsigned int mt6358_upmu_set_rg_en_record(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_spi_rsv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_dew_dio_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_dew_read_test(
	void);
extern unsigned int mt6358_upmu_set_dew_write_test(
	unsigned int val);
extern unsigned int mt6358_upmu_set_dew_crc_swrst(
	unsigned int val);
extern unsigned int mt6358_upmu_set_dew_crc_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_dew_crc_val(
	void);
extern unsigned int mt6358_upmu_set_dew_dbg_mon_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_dew_cipher_key_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_dew_cipher_iv_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_dew_cipher_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_dew_cipher_rdy(
	void);
extern unsigned int mt6358_upmu_set_dew_cipher_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_dew_cipher_swrst(
	unsigned int val);
extern unsigned int mt6358_upmu_set_dew_rddmy_no(
	unsigned int val);
extern unsigned int mt6358_upmu_set_int_type_con0(
	unsigned int val);
extern unsigned int mt6358_upmu_get_cpu_int_sta(
	void);
extern unsigned int mt6358_upmu_get_md32_int_sta(
	void);
extern unsigned int mt6358_upmu_set_rg_srclken_in3_smps_clk_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_srclken_in3_en_smps_test(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_srclken_in3_en_smps_test(
	void);
extern unsigned int mt6358_upmu_set_rg_srclken_in2_smps_clk_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_srclken_in2_en_smps_test(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_srclken_in2_en_smps_test(
	void);
extern unsigned int mt6358_upmu_set_rg_spi_dly_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_get_record_cmd0(
	void);
extern unsigned int mt6358_upmu_get_record_cmd1(
	void);
extern unsigned int mt6358_upmu_get_record_cmd2(
	void);
extern unsigned int mt6358_upmu_get_record_wdata0(
	void);
extern unsigned int mt6358_upmu_get_record_wdata1(
	void);
extern unsigned int mt6358_upmu_get_record_wdata2(
	void);
extern unsigned int mt6358_upmu_set_rg_addr_target(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_addr_mask(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_wdata_target(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_wdata_mask(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_spi_record_clr(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_cmd_alert_clr(
	unsigned int val);
extern unsigned int mt6358_upmu_set_spislv_key(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_srclken_in2_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_srclken_in2_en(
	void);
extern unsigned int mt6358_upmu_set_rg_srclken_in3_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_srclken_in3_en(
	void);
extern unsigned int mt6358_upmu_get_sck_top_ana_id(
	void);
extern unsigned int mt6358_upmu_get_sck_top_dig_id(
	void);
extern unsigned int mt6358_upmu_get_sck_top_ana_minor_rev(
	void);
extern unsigned int mt6358_upmu_get_sck_top_ana_major_rev(
	void);
extern unsigned int mt6358_upmu_get_sck_top_dig_minor_rev(
	void);
extern unsigned int mt6358_upmu_get_sck_top_dig_major_rev(
	void);
extern unsigned int mt6358_upmu_get_sck_top_cbs(
	void);
extern unsigned int mt6358_upmu_get_sck_top_bix(
	void);
extern unsigned int mt6358_upmu_set_sck_top_xtal_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_sck_top_reserved(
	unsigned int val);
extern unsigned int mt6358_upmu_get_xosc32_enb_det(
	void);
extern unsigned int mt6358_upmu_get_sck_top_test_out(
	void);
extern unsigned int mt6358_upmu_set_sck_top_mon_flag_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_sck_top_mon_grp_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_fqmtr_tcksel(
	unsigned int val);
extern unsigned int mt6358_upmu_get_fqmtr_busy(
	void);
extern unsigned int mt6358_upmu_set_fqmtr_dcxo26m_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_fqmtr_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_fqmtr_winset(
	unsigned int val);
extern unsigned int mt6358_upmu_get_fqmtr_data(
	void);
extern unsigned int mt6358_upmu_set_rg_rtc_sec_mclk_pdn(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_eosc_cali_test_ck_pdn(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_rtc_eosc32_ck_pdn(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_rtc_sec_32k_ck_pdn(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_rtc_mclk_pdn(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_rtc_32k_ck_pdn(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_rtc_26m_ck_pdn(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_rtc_2sec_off_det_pdn(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_rtc_intrp_ck_pdn(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_rtc_26m_ck_pdn_hwen(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_rtc_mclk_pdn_hwen(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_rtc_sec_32k_ck_pdn_hwen(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_rtc_sec_mclk_pdn_hwen(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_rtc_intrp_ck_pdn_hwen(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_rtc_clk_pdn_hwen_rsv_1(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_rtc_clk_pdn_hwen_rsv_0(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_rtc_ck_tstsel_rsv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_rtcdet_ck_tstsel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_eosc_cali_test_ck_tstsel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_rtc_eosc32_ck_tstsel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_rtc_swrst(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_rtc_sec_swrst(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_bank_rtc_swrst(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_bank_rtc_sec_swrst(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_bank_eosc_cali_swrst(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_bank_sck_top_swrst(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_en_rtc(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_rtc(
	void);
extern unsigned int mt6358_upmu_set_rg_int_mask_rtc(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_rtc(
	void);
extern unsigned int mt6358_upmu_set_sck_top_polarity(
	unsigned int val);
extern unsigned int mt6358_upmu_set_eosc_cali_start(
	unsigned int val);
extern unsigned int mt6358_upmu_set_eosc_cali_td(
	unsigned int val);
extern unsigned int mt6358_upmu_set_eosc_cali_test(
	unsigned int val);
extern unsigned int mt6358_upmu_set_eosc_cali_dcxo_rdy_td(
	unsigned int val);
extern unsigned int mt6358_upmu_set_frc_vtcxo0_on(
	unsigned int val);
extern unsigned int mt6358_upmu_set_eosc_cali_rsv(
	unsigned int val);
extern unsigned int mt6358_upmu_get_mix_eosc32_stp_lpdtb(
	void);
extern unsigned int mt6358_upmu_set_mix_eosc32_stp_lpden(
	unsigned int val);
extern unsigned int mt6358_upmu_set_mix_xosc32_stp_pwdb(
	unsigned int val);
extern unsigned int mt6358_upmu_get_mix_xosc32_stp_lpdtb(
	void);
extern unsigned int mt6358_upmu_set_mix_xosc32_stp_lpden(
	unsigned int val);
extern unsigned int mt6358_upmu_set_mix_xosc32_stp_lpdrst(
	unsigned int val);
extern unsigned int mt6358_upmu_set_mix_xosc32_stp_cali(
	unsigned int val);
extern unsigned int mt6358_upmu_set_stmp_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_mix_eosc32_stp_chop_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_mix_dcxo_stp_lvsh_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_mix_pmu_stp_ddlo_vrtc(
	unsigned int val);
extern unsigned int mt6358_upmu_set_mix_pmu_stp_ddlo_vrtc_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_mix_rtc_stp_xosc32_enb(
	unsigned int val);
extern unsigned int mt6358_upmu_set_mix_dcxo_stp_test_deglitch_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_mix_eosc32_stp_rsv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_mix_eosc32_vct_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_mix_eosc32_opt(
	unsigned int val);
extern unsigned int mt6358_upmu_set_mix_dcxo_stp_lvsh_en_int(
	unsigned int val);
extern unsigned int mt6358_upmu_set_mix_rtc_gpio_coredetb(
	unsigned int val);
extern unsigned int mt6358_upmu_set_mix_rtc_gpio_f32kob(
	unsigned int val);
extern unsigned int mt6358_upmu_set_mix_rtc_gpio_gpo(
	unsigned int val);
extern unsigned int mt6358_upmu_set_mix_rtc_gpio_oe(
	unsigned int val);
extern unsigned int mt6358_upmu_get_mix_rtc_stp_debug_out(
	void);
extern unsigned int mt6358_upmu_set_mix_rtc_stp_debug_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_mix_rtc_stp_k_eosc32_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_mix_rtc_stp_embck_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_mix_stp_bbwakeup(
	unsigned int val);
extern unsigned int mt6358_upmu_get_mix_stp_rtc_ddlo(
	void);
extern unsigned int mt6358_upmu_get_mix_rtc_xosc32_enb(
	void);
extern unsigned int mt6358_upmu_set_mix_efuse_xosc32_enb_opt(
	unsigned int val);
extern unsigned int mt6358_upmu_set_pwren(
	unsigned int val);
extern unsigned int mt6358_upmu_set_bbpu_clr(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auto(
	unsigned int val);
extern unsigned int mt6358_upmu_get_cbusy(
	void);
extern unsigned int mt6358_upmu_get_alsta(
	void);
extern unsigned int mt6358_upmu_get_tcsta(
	void);
extern unsigned int mt6358_upmu_get_lpsta(
	void);
extern unsigned int mt6358_upmu_set_al_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_tc_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_oneshot(
	unsigned int val);
extern unsigned int mt6358_upmu_set_lp_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_seccii(
	unsigned int val);
extern unsigned int mt6358_upmu_set_mincii(
	unsigned int val);
extern unsigned int mt6358_upmu_set_houcii(
	unsigned int val);
extern unsigned int mt6358_upmu_set_domcii(
	unsigned int val);
extern unsigned int mt6358_upmu_set_dowcii(
	unsigned int val);
extern unsigned int mt6358_upmu_set_mthcii(
	unsigned int val);
extern unsigned int mt6358_upmu_set_yeacii(
	unsigned int val);
extern unsigned int mt6358_upmu_set_seccii_1_2(
	unsigned int val);
extern unsigned int mt6358_upmu_set_seccii_1_4(
	unsigned int val);
extern unsigned int mt6358_upmu_set_seccii_1_8(
	unsigned int val);
extern unsigned int mt6358_upmu_set_sec_msk(
	unsigned int val);
extern unsigned int mt6358_upmu_set_min_msk(
	unsigned int val);
extern unsigned int mt6358_upmu_set_hou_msk(
	unsigned int val);
extern unsigned int mt6358_upmu_set_dom_msk(
	unsigned int val);
extern unsigned int mt6358_upmu_set_dow_msk(
	unsigned int val);
extern unsigned int mt6358_upmu_set_mth_msk(
	unsigned int val);
extern unsigned int mt6358_upmu_set_yea_msk(
	unsigned int val);
extern unsigned int mt6358_upmu_set_tc_second(
	unsigned int val);
extern unsigned int mt6358_upmu_set_tc_minute(
	unsigned int val);
extern unsigned int mt6358_upmu_set_tc_hour(
	unsigned int val);
extern unsigned int mt6358_upmu_set_tc_dom(
	unsigned int val);
extern unsigned int mt6358_upmu_set_tc_dow(
	unsigned int val);
extern unsigned int mt6358_upmu_set_tc_month(
	unsigned int val);
extern unsigned int mt6358_upmu_set_tc_year(
	unsigned int val);
extern unsigned int mt6358_upmu_set_al_second(
	unsigned int val);
extern unsigned int mt6358_upmu_set_bbpu_auto_pdn_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_bbpu_2sec_ck_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_bbpu_2sec_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_bbpu_2sec_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_get_bbpu_2sec_stat_sta(
	void);
extern unsigned int mt6358_upmu_set_rtc_lpd_opt(
	unsigned int val);
extern unsigned int mt6358_upmu_set_k_eosc32_vtcxo_on_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_al_minute(
	unsigned int val);
extern unsigned int mt6358_upmu_set_al_hour(
	unsigned int val);
extern unsigned int mt6358_upmu_set_new_spare0(
	unsigned int val);
extern unsigned int mt6358_upmu_set_al_dom(
	unsigned int val);
extern unsigned int mt6358_upmu_set_new_spare1(
	unsigned int val);
extern unsigned int mt6358_upmu_set_al_dow(
	unsigned int val);
extern unsigned int mt6358_upmu_set_new_spare2(
	unsigned int val);
extern unsigned int mt6358_upmu_set_al_month(
	unsigned int val);
extern unsigned int mt6358_upmu_set_new_spare3(
	unsigned int val);
extern unsigned int mt6358_upmu_set_al_year(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rtc_k_eosc_rsv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xosccali(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rtc_xosc32_enb(
	void);
extern unsigned int mt6358_upmu_set_rtc_embck_sel_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rtc_embck_src_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rtc_embck_sel_option(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rtc_gps_ckout_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rtc_eosc32_vct_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rtc_eosc32_chop_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rtc_gp_osc32_con(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rtc_reg_xosc32_enb(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rtc_powerkey1(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rtc_powerkey2(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rtc_pdn1(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rtc_pdn2(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rtc_spar0(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rtc_spar1(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rtc_prot(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rtc_diff(
	unsigned int val);
extern unsigned int mt6358_upmu_get_power_detected(
	void);
extern unsigned int mt6358_upmu_set_k_eosc32_rsv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_cali_rd_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rtc_cali(
	unsigned int val);
extern unsigned int mt6358_upmu_set_cali_wr_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_k_eosc32_overflow(
	unsigned int val);
extern unsigned int mt6358_upmu_set_vbat_lpsta_raw(
	unsigned int val);
extern unsigned int mt6358_upmu_set_eosc32_lpen(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xosc32_lpen(
	unsigned int val);
extern unsigned int mt6358_upmu_set_lprst(
	unsigned int val);
extern unsigned int mt6358_upmu_set_cdbo(
	unsigned int val);
extern unsigned int mt6358_upmu_set_f32kob(
	unsigned int val);
extern unsigned int mt6358_upmu_set_gpo(
	unsigned int val);
extern unsigned int mt6358_upmu_set_goe(
	unsigned int val);
extern unsigned int mt6358_upmu_set_gsr(
	unsigned int val);
extern unsigned int mt6358_upmu_set_gsmt(
	unsigned int val);
extern unsigned int mt6358_upmu_set_gpen(
	unsigned int val);
extern unsigned int mt6358_upmu_set_gpu(
	unsigned int val);
extern unsigned int mt6358_upmu_set_ge4(
	unsigned int val);
extern unsigned int mt6358_upmu_set_ge8(
	unsigned int val);
extern unsigned int mt6358_upmu_get_gpi(
	void);
extern unsigned int mt6358_upmu_get_lpsta_raw(
	void);
extern unsigned int mt6358_upmu_get_rtc_int_cnt(
	void);
extern unsigned int mt6358_upmu_set_rtc_sec_dat0(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rtc_sec_dat1(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rtc_sec_dat2(
	unsigned int val);
extern unsigned int mt6358_upmu_set_tc_second_sec(
	unsigned int val);
extern unsigned int mt6358_upmu_set_tc_minute_sec(
	unsigned int val);
extern unsigned int mt6358_upmu_set_tc_hour_sec(
	unsigned int val);
extern unsigned int mt6358_upmu_set_tc_dom_sec(
	unsigned int val);
extern unsigned int mt6358_upmu_set_tc_dow_sec(
	unsigned int val);
extern unsigned int mt6358_upmu_set_tc_month_sec(
	unsigned int val);
extern unsigned int mt6358_upmu_set_tc_year_sec(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rtc_sec_ck_pdn(
	unsigned int val);
extern unsigned int mt6358_upmu_get_dcxo_dsn_cbs(
	void);
extern unsigned int mt6358_upmu_get_dcxo_dsn_bix(
	void);
extern unsigned int mt6358_upmu_get_dcxo_dsn_esp(
	void);
extern unsigned int mt6358_upmu_get_dcxo_dsn_fpi(
	void);
extern unsigned int mt6358_upmu_set_xo_extbuf1_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_extbuf1_en_m(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_extbuf2_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_extbuf2_en_m(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_extbuf3_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_extbuf3_en_m(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_extbuf4_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_extbuf4_en_m(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_bb_lpm_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_enbb_man(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_enbb_en_m(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_clksel_man(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_clksel_en_m(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_extbuf1_ckg_man(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_extbuf1_ckg_en_m(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_extbuf2_ckg_man(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_extbuf2_ckg_en_m(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_extbuf3_ckg_man(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_extbuf3_ckg_en_m(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_extbuf4_ckg_man(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_extbuf4_ckg_en_m(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_intbuf_man(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_pbuf_en_m(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_ibuf_en_m(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_lpmbuf_man(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_lpm_prebuf_en_m(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_lpbuf_en_m(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_bblpm_cksel_m(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_en32k_man(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_en32k_m(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_xmode_man(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_xmode_m(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_strup_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_aac_fpm_time(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_aac_mode_lpm(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_aac_mode_fpm(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_en26m_offsq_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_ldocal_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_cbank_sync_dyn(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_26mlp_man_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_bufldok_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_pmu_cken_m(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_pmu_cken_man(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_extbuf6_ckg_man(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_extbuf6_ckg_en_m(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_extbuf7_ckg_man(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_extbuf7_ckg_en_m(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_lpm_isel_man(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_fpm_isel_man(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_cdac_fpm(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_cdac_lpm(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_32kdiv_nfrac_fpm(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_cofst_fpm(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_32kdiv_nfrac_lpm(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_cofst_lpm(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_core_man(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_core_en_m(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_core_turbo_en_m(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_core_aac_en_m(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_startup_en_m(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_core_vbfpm_en_m(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_core_vblpm_en_m(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_lpmbias_en_m(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_vtcgen_en_m(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_iaac_comp_en_m(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_ifpm_comp_en_m(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_ilpm_comp_en_m(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_core_bypcas_fpm(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_core_gmx2_fpm(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_core_idac_fpm(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_aac_comp_man(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_aac_en_m(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_aac_monen_m(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_comp_en_m(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_comp_tsten_m(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_aac_hv_fpm(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_aac_ibias_fpm(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_aac_vofst_fpm(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_aac_comp_hv_fpm(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_aac_vsel_fpm(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_aac_comp_pol(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_core_bypcas_lpm(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_core_gmx2_lpm(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_core_idac_lpm(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_aac_comp_hv_lpm(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_aac_vsel_lpm(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_aac_hv_lpm(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_aac_ibias_lpm(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_aac_vofst_lpm(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_aac_fpm_swen(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_swrst(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_32kdiv_swrst(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_32kdiv_ratio_man(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_32kdiv_test_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_cbank_sync_man(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_cbank_sync_en_m(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_ctl_sync_man(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_ctl_sync_en_m(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_ldo_man(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_ldopbuf_en_m(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_ldopbuf_vset_m(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_ldovtst_en_m(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_test_vcal_en_m(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_vbist_en_m(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_vtest_sel_mux(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_reserved3(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_extbuf6_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_extbuf6_en_m(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_extbuf7_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_extbuf7_en_m(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_bufldok_man(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_buf1ldo_cal_m(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_bufldo_cal_m(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_extbuf4_clksel_man(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_vio18pg_bufen(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_cal_en_man(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_cal_en_m(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_xo_core_osctd(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_thadc_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_xo_sync_ckpol(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_xo_cbank_pol(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_xo_cbank_sync_byp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_xo_ctl_pol(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_xo_ctl_sync_byp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_xo_lpbuf_inv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_xo_ldopbuf_byp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_xo_ldopbuf_encl(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_xo_ldopbuf_encl(
	void);
extern unsigned int mt6358_upmu_set_rg_xo_vgbias_vset(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_xo_pbuf_iset(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_xo_ibuf_iset(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_xo_reserved4(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_xo_vow_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_xo_vow_en(
	void);
extern unsigned int mt6358_upmu_set_rg_xo_vow_div(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_xo_bufldo24_encl(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_xo_bufldo24_encl(
	void);
extern unsigned int mt6358_upmu_set_rg_xo_bufldo24_ibx2(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_xo_reserved5(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_xo_bufldo13_encl(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_xo_bufldo13_encl(
	void);
extern unsigned int mt6358_upmu_set_rg_xo_bufldo13_ibx2(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_xo_bufldo13_ix2(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_xo_lvldo_i_ctrl(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_xo_bufldo67_encl(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_xo_bufldo67_encl(
	void);
extern unsigned int mt6358_upmu_set_rg_xo_bufldo67_ibx2(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_xo_bufldo67_ix2(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_xo_lvldo_rfb(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_xo_extbuf_inv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_xo_reserved0(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_extbuf2_clksel_man(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_audio_en_m(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_xo_audio_atten(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_xo_audio_iset(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_xo_extbuf1_hd(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_xo_extbuf2_hd(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_xo_extbuf3_hd(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_xo_extbuf4_hd(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_xo_reserved8(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_xo_extbuf6_hd(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_xo_extbuf7_hd(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_extbuf1_iset_m(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_extbuf2_iset_m(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_extbuf3_iset_m(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_extbuf4_iset_m(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_reserved9(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_extbuf6_iset_m(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_extbuf7_iset_m(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_xo_lpm_prebuf_iset(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_xo_reserved1(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_thadc_en_man(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_xo_tsource_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_xo_tsource_en(
	void);
extern unsigned int mt6358_upmu_set_xo_bufldo13_vset_m(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_bufldo24_vset_m(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_bufldo67_vset_m(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_static_auxout_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_auxout_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_get_xo_static_auxout(
	void);
extern unsigned int mt6358_upmu_set_rg_xo_pctat_comp_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_xo_pctat_comp_en(
	void);
extern unsigned int mt6358_upmu_set_rg_xo_heater_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_xo_corner_detect_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_xo_corner_detect_en(
	void);
extern unsigned int mt6358_upmu_set_rg_xo_corner_detect_en_man(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_xo_corner_detect_en_man(
	void);
extern unsigned int mt6358_upmu_set_rg_xo_resrved10(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_xo_corner_setting_tune(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_xo_resrved11(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rgs_ad_xo_corner_cal_done(
	void);
extern unsigned int mt6358_upmu_get_rgs_ad_xo_corner_sel(
	void);
extern unsigned int mt6358_upmu_set_xo_mdb_tbo_en_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_ptatctat_en_man(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_ptatctat_en_m(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_ptatctat_en_lpm(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_ptatctat_en_fpm(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_bb_lpm_cel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_buf1_bblpm_en_mask(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_buf2_bblpm_en_mask(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_buf3_bblpm_en_mask(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_buf4_bblpm_en_mask(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_buf6_bblpm_en_mask(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_buf7_bblpm_en_mask(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_pmu_cken_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_reserved2(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_xo_resrved12(
	unsigned int val);
extern unsigned int mt6358_upmu_get_dcxo_elr_len(
	void);
extern unsigned int mt6358_upmu_set_rg_xo_pctat_rdeg_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_xo_gs_vtemp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_pwrkey_rstb_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_get_psc_top_ana_id(
	void);
extern unsigned int mt6358_upmu_get_psc_top_dig_id(
	void);
extern unsigned int mt6358_upmu_get_psc_top_ana_minor_rev(
	void);
extern unsigned int mt6358_upmu_get_psc_top_ana_major_rev(
	void);
extern unsigned int mt6358_upmu_get_psc_top_dig_minor_rev(
	void);
extern unsigned int mt6358_upmu_get_psc_top_dig_major_rev(
	void);
extern unsigned int mt6358_upmu_get_psc_top_cbs(
	void);
extern unsigned int mt6358_upmu_get_psc_top_bix(
	void);
extern unsigned int mt6358_upmu_get_psc_top_esp(
	void);
extern unsigned int mt6358_upmu_get_psc_top_fpi(
	void);
extern unsigned int mt6358_upmu_get_psc_top_clk_offset(
	void);
extern unsigned int mt6358_upmu_get_psc_top_rst_offset(
	void);
extern unsigned int mt6358_upmu_get_psc_top_int_offset(
	void);
extern unsigned int mt6358_upmu_get_psc_top_int_len(
	void);
extern unsigned int mt6358_upmu_set_rg_psc_intrp_ck_pdn_hwen(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_psc_intrp_ck_pdn(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_chrdet_32k_ck_pdn(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_strup_long_press_rst(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_pseq_pwrmsk_rst_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_bank_strup_swrst(
	unsigned int val);
extern unsigned int mt6358_upmu_set_bank_pseq_swrst(
	unsigned int val);
extern unsigned int mt6358_upmu_set_bank_chrdet_swrst(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_chrdet_rst(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_en_pwrkey(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_pwrkey(
	void);
extern unsigned int mt6358_upmu_set_rg_int_en_homekey(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_homekey(
	void);
extern unsigned int mt6358_upmu_set_rg_int_en_pwrkey_r(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_pwrkey_r(
	void);
extern unsigned int mt6358_upmu_set_rg_int_en_homekey_r(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_homekey_r(
	void);
extern unsigned int mt6358_upmu_set_rg_int_en_ni_lbat_int(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_ni_lbat_int(
	void);
extern unsigned int mt6358_upmu_set_rg_int_en_chrdet(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_chrdet(
	void);
extern unsigned int mt6358_upmu_set_rg_int_en_chrdet_edge(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_chrdet_edge(
	void);
extern unsigned int mt6358_upmu_set_rg_int_en_vcdt_hv_det(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_vcdt_hv_det(
	void);
extern unsigned int mt6358_upmu_set_rg_int_mask_pwrkey(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_homekey(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_pwrkey_r(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_homekey_r(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_ni_lbat_int(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_chrdet(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_chrdet_edge(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_vcdt_hv_det(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_pwrkey(
	void);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_homekey(
	void);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_pwrkey_r(
	void);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_homekey_r(
	void);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_ni_lbat_int(
	void);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_chrdet(
	void);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_chrdet_edge(
	void);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_vcdt_hv_det(
	void);
extern unsigned int mt6358_upmu_set_rg_psc_int_polarity(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_homekey_int_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_pwrkey_int_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_psc_mon_grp_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_get_strup_ana_id(
	void);
extern unsigned int mt6358_upmu_get_strup_dig_id(
	void);
extern unsigned int mt6358_upmu_get_strup_ana_minor_rev(
	void);
extern unsigned int mt6358_upmu_get_strup_ana_major_rev(
	void);
extern unsigned int mt6358_upmu_get_strup_dig_minor_rev(
	void);
extern unsigned int mt6358_upmu_get_strup_dig_major_rev(
	void);
extern unsigned int mt6358_upmu_get_strup_cbs(
	void);
extern unsigned int mt6358_upmu_get_strup_bix(
	void);
extern unsigned int mt6358_upmu_set_rg_tm_out(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_thrdet_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_strup_thr_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_thr_tmode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vref_bg(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_rst_drvsel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_en1_drvsel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_en2_drvsel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_pmu_rsv(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rgs_ana_chip_id(
	void);
extern unsigned int mt6358_upmu_set_rg_strup_iref_trim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_thr_loc_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_get_pseq_ana_id(
	void);
extern unsigned int mt6358_upmu_get_pseq_dig_id(
	void);
extern unsigned int mt6358_upmu_get_pseq_ana_minor_rev(
	void);
extern unsigned int mt6358_upmu_get_pseq_ana_major_rev(
	void);
extern unsigned int mt6358_upmu_get_pseq_dig_major_rev(
	void);
extern unsigned int mt6358_upmu_get_pseq_cbs(
	void);
extern unsigned int mt6358_upmu_get_pseq_bix(
	void);
extern unsigned int mt6358_upmu_get_pseq_fpi(
	void);
extern unsigned int mt6358_upmu_set_rg_pwrhold(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_usbdl_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_crst(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_wrst(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_crst_intv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_wrst_intv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_pseq_pg_ck_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_pseq_spar_xcpt_mask(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_pseq_rtca_xcpt_mask(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_thm_shdn_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_thm_shdn_en(
	void);
extern unsigned int mt6358_upmu_set_rg_wdtrst_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_wdtrst_en(
	void);
extern unsigned int mt6358_upmu_set_rg_wdtrst_act(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_keypwr_vcore_opt(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_pseq_force_on(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_pseq_force_all_doff(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_por_flag(
	unsigned int val);
extern unsigned int mt6358_upmu_get_usbdl(
	void);
extern unsigned int mt6358_upmu_set_rg_thr_test(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_strup_ther_deb_rtd(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_strup_ther_deb_ftd(
	unsigned int val);
extern unsigned int mt6358_upmu_set_dduvlo_deb_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_strup_pg_deb_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_strup_osc_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_strup_osc_en(
	void);
extern unsigned int mt6358_upmu_set_rg_strup_osc_en_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_strup_osc_en_sel(
	void);
extern unsigned int mt6358_upmu_set_rg_strup_ft_ctrl(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_strup_pwron_force(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_biasgen_force(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_strup_pwron(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_strup_pwron_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_biasgen(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_biasgen_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_rtc_xosc32_enb(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_rtc_xosc32_enb(
	void);
extern unsigned int mt6358_upmu_set_rg_rtc_xosc32_enb_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_rtc_xosc32_enb_sel(
	void);
extern unsigned int mt6358_upmu_set_strup_dig_io_pg_force(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_clr_just_smart_rst(
	unsigned int val);
extern unsigned int mt6358_upmu_set_clr_just_rst(
	unsigned int val);
extern unsigned int mt6358_upmu_get_just_smart_rst(
	void);
extern unsigned int mt6358_upmu_get_just_pwrkey_rst(
	void);
extern unsigned int mt6358_upmu_get_da_qi_osc_en(
	void);
extern unsigned int mt6358_upmu_set_rg_strup_ext_pmic_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_strup_ext_pmic_en(
	void);
extern unsigned int mt6358_upmu_set_rg_strup_ext_pmic_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_get_da_ext_pmic_en1(
	void);
extern unsigned int mt6358_upmu_get_da_ext_pmic_en2(
	void);
extern unsigned int mt6358_upmu_set_rg_strup_auxadc_start_sw(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_strup_auxadc_rstb_sw(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_strup_auxadc_start_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_strup_auxadc_rstb_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_strup_auxadc_rpcnt_max(
	unsigned int val);
extern unsigned int mt6358_upmu_set_strup_pwroff_seq_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_strup_pwroff_preoff_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_slot_intv_down(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_dseq_len(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vxo22_dsa(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vaux18_dsa(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vs2_dsa(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vcore_dsa(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vmodem_dsa(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vgpu_dsa(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vsram_others_dsa(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vsram_gpu_dsa(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ext_pmic_2_dsa(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vs1_dsa(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_va12_dsa(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vproc11_dsa(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vproc12_dsa(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vsram_proc11_dsa(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vsram_proc12_dsa(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vio18_dsa(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vemc_dsa(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ext_pmic_1_dsa(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vio28_dsa(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vdram1_dsa(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vdram2_dsa(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vusb_dsa(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vaud28_dsa(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_rsv_dsa(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_rsv_swreg(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_strup_uvlo_u1u2_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_strup_uvlo_u1u2_sel_swctrl(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_uvlo_dec_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_uvlo_dec_en(
	void);
extern unsigned int mt6358_upmu_set_rg_strup_thr_clr(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_strup_long_press_ext_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_strup_long_press_ext_td(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_strup_long_press_ext_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_strup_long_press_ext_en(
	void);
extern unsigned int mt6358_upmu_set_rg_strup_long_press_ext_chr_ctrl(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_strup_long_press_ext_pwrkey_ctrl(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_strup_long_press_ext_spar_ctrl(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_strup_long_press_ext_rtca_ctrl(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_smart_rst_sdn_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_smart_rst_sdn_en(
	void);
extern unsigned int mt6358_upmu_set_rg_smart_rst_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_strup_envtem(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_strup_envtem(
	void);
extern unsigned int mt6358_upmu_set_rg_strup_envtem_ctrl(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_strup_envtem_ctrl(
	void);
extern unsigned int mt6358_upmu_set_rg_strup_pwrkey_count_reset(
	unsigned int val);
extern unsigned int mt6358_upmu_get_pup_pkey_release(
	void);
extern unsigned int mt6358_upmu_get_pwrkey_long_press_count(
	void);
extern unsigned int mt6358_upmu_set_rg_rtc_spar_deb_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_rtc_spar_deb_en(
	void);
extern unsigned int mt6358_upmu_set_rg_rtc_alarm_deb_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_rtc_alarm_deb_en(
	void);
extern unsigned int mt6358_upmu_set_rg_strup_vdram2_pg_h2l_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_strup_vdram2_pg_h2l_en(
	void);
extern unsigned int mt6358_upmu_set_rg_strup_vemc_pg_h2l_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_strup_vemc_pg_h2l_en(
	void);
extern unsigned int mt6358_upmu_set_rg_strup_vsram_proc12_pg_h2l_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_strup_vsram_proc12_pg_h2l_en(
	void);
extern unsigned int mt6358_upmu_set_rg_strup_vsram_proc11_pg_h2l_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_strup_vsram_proc11_pg_h2l_en(
	void);
extern unsigned int mt6358_upmu_set_rg_strup_va12_pg_h2l_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_strup_va12_pg_h2l_en(
	void);
extern unsigned int mt6358_upmu_set_rg_strup_vsram_gpu_pg_h2l_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_strup_vsram_gpu_pg_h2l_en(
	void);
extern unsigned int mt6358_upmu_set_rg_strup_vsram_others_pg_h2l_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_strup_vsram_others_pg_h2l_en(
	void);
extern unsigned int mt6358_upmu_set_rg_strup_vaux18_pg_h2l_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_strup_vaux18_pg_h2l_en(
	void);
extern unsigned int mt6358_upmu_set_rg_strup_vdram1_pg_h2l_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_strup_vdram1_pg_h2l_en(
	void);
extern unsigned int mt6358_upmu_set_rg_strup_vproc12_pg_h2l_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_strup_vproc12_pg_h2l_en(
	void);
extern unsigned int mt6358_upmu_set_rg_strup_vproc11_pg_h2l_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_strup_vproc11_pg_h2l_en(
	void);
extern unsigned int mt6358_upmu_set_rg_strup_vs1_pg_h2l_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_strup_vs1_pg_h2l_en(
	void);
extern unsigned int mt6358_upmu_set_rg_strup_vgpu_pg_h2l_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_strup_vgpu_pg_h2l_en(
	void);
extern unsigned int mt6358_upmu_set_rg_strup_vmodem_pg_h2l_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_strup_vmodem_pg_h2l_en(
	void);
extern unsigned int mt6358_upmu_set_rg_strup_vcore_pg_h2l_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_strup_vcore_pg_h2l_en(
	void);
extern unsigned int mt6358_upmu_set_rg_strup_vs2_pg_h2l_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_strup_vs2_pg_h2l_en(
	void);
extern unsigned int mt6358_upmu_set_rg_strup_rsv_pg_h2l_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_strup_rsv_pg_h2l_en(
	void);
extern unsigned int mt6358_upmu_set_rg_strup_vaud28_pg_h2l_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_strup_vaud28_pg_h2l_en(
	void);
extern unsigned int mt6358_upmu_set_rg_strup_vusb_pg_h2l_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_strup_vusb_pg_h2l_en(
	void);
extern unsigned int mt6358_upmu_set_rg_strup_vio18_pg_enb(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_strup_vio18_pg_enb(
	void);
extern unsigned int mt6358_upmu_set_rg_strup_vsram_proc12_pg_enb(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_strup_vsram_proc12_pg_enb(
	void);
extern unsigned int mt6358_upmu_set_rg_strup_vsram_proc11_pg_enb(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_strup_vsram_proc11_pg_enb(
	void);
extern unsigned int mt6358_upmu_set_rg_strup_va12_pg_enb(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_strup_va12_pg_enb(
	void);
extern unsigned int mt6358_upmu_set_rg_strup_vsram_gpu_pg_enb(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_strup_vsram_gpu_pg_enb(
	void);
extern unsigned int mt6358_upmu_set_rg_strup_vsram_others_pg_enb(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_strup_vsram_others_pg_enb(
	void);
extern unsigned int mt6358_upmu_set_rg_strup_vaux18_pg_enb(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_strup_vaux18_pg_enb(
	void);
extern unsigned int mt6358_upmu_set_rg_strup_vxo22_pg_enb(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_strup_vxo22_pg_enb(
	void);
extern unsigned int mt6358_upmu_set_rg_strup_vdram1_pg_enb(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_strup_vdram1_pg_enb(
	void);
extern unsigned int mt6358_upmu_set_rg_strup_vproc12_pg_enb(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_strup_vproc12_pg_enb(
	void);
extern unsigned int mt6358_upmu_set_rg_strup_vproc11_pg_enb(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_strup_vproc11_pg_enb(
	void);
extern unsigned int mt6358_upmu_set_rg_strup_vs1_pg_enb(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_strup_vs1_pg_enb(
	void);
extern unsigned int mt6358_upmu_set_rg_strup_vgpu_pg_enb(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_strup_vgpu_pg_enb(
	void);
extern unsigned int mt6358_upmu_set_rg_strup_vmodem_pg_enb(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_strup_vmodem_pg_enb(
	void);
extern unsigned int mt6358_upmu_set_rg_strup_vcore_pg_enb(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_strup_vcore_pg_enb(
	void);
extern unsigned int mt6358_upmu_set_rg_strup_vs2_pg_enb(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_strup_vs2_pg_enb(
	void);
extern unsigned int mt6358_upmu_set_rg_strup_rsv_pg_enb(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_strup_rsv_pg_enb(
	void);
extern unsigned int mt6358_upmu_set_rg_strup_vaud28_pg_enb(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_strup_vaud28_pg_enb(
	void);
extern unsigned int mt6358_upmu_set_rg_strup_vusb_pg_enb(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_strup_vusb_pg_enb(
	void);
extern unsigned int mt6358_upmu_set_rg_strup_vdram2_pg_enb(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_strup_vdram2_pg_enb(
	void);
extern unsigned int mt6358_upmu_set_rg_strup_vio28_pg_enb(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_strup_vio28_pg_enb(
	void);
extern unsigned int mt6358_upmu_set_rg_strup_vemc_pg_enb(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_strup_vemc_pg_enb(
	void);
extern unsigned int mt6358_upmu_set_rg_strup_vdram1_oc_enb(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_strup_vdram1_oc_enb(
	void);
extern unsigned int mt6358_upmu_set_rg_strup_vproc12_oc_enb(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_strup_vproc12_oc_enb(
	void);
extern unsigned int mt6358_upmu_set_rg_strup_vproc11_oc_enb(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_strup_vproc11_oc_enb(
	void);
extern unsigned int mt6358_upmu_set_rg_strup_vs1_oc_enb(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_strup_vs1_oc_enb(
	void);
extern unsigned int mt6358_upmu_set_rg_strup_vgpu_oc_enb(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_strup_vgpu_oc_enb(
	void);
extern unsigned int mt6358_upmu_set_rg_strup_vmodem_oc_enb(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_strup_vmodem_oc_enb(
	void);
extern unsigned int mt6358_upmu_set_rg_strup_vcore_oc_enb(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_strup_vcore_oc_enb(
	void);
extern unsigned int mt6358_upmu_set_rg_strup_vs2_oc_enb(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_strup_vs2_oc_enb(
	void);
extern unsigned int mt6358_upmu_get_pseq_elr_len(
	void);
extern unsigned int mt6358_upmu_set_rg_vxo22_usa(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vaux18_usa(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vs2_usa(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vcore_usa(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vmodem_usa(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vgpu_usa(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vsram_others_usa(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vsram_gpu_usa(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ext_pmic_2_usa(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vs1_usa(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_va12_usa(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vproc11_usa(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vproc12_usa(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vsram_proc11_usa(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vsram_proc12_usa(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vio18_usa(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vemc_usa(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ext_pmic_1_usa(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vio28_usa(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vdram1_usa(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vdram2_usa(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vusb_usa(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vaud28_usa(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_rsv_usa(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_bwdt_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_bwdt_en(
	void);
extern unsigned int mt6358_upmu_set_rg_bwdt_tsel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_bwdt_csel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_bwdt_td(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_bwdt_chrtd(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_bwdt_ddlo_td(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_bwdt_srcsel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_slot_intv_up(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_seq_len(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_pspg_shdn_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_pspg_shdn_en(
	void);
extern unsigned int mt6358_upmu_set_rg_rstb_onintv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_pseq_f75k_force(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_pseq_1ms_tk_ext(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_smps_ivgen_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_strup_long_press_reset_extend(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_cps_s0ext_enb(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_cps_s0ext_enb(
	void);
extern unsigned int mt6358_upmu_set_rg_cps_s0ext_td(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_sdn_dly_enb(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_sdn_dly_enb(
	void);
extern unsigned int mt6358_upmu_set_rg_chrdet_deb_td(
	unsigned int val);
extern unsigned int mt6358_upmu_set_efuse_ivgen_enb_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_pwrkey_event_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_pwrkey_event_mode_hw(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_pg_stb_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_get_chrdet_ana_id(
	void);
extern unsigned int mt6358_upmu_get_chrdet_dig_id(
	void);
extern unsigned int mt6358_upmu_get_chrdet_ana_minor_rev(
	void);
extern unsigned int mt6358_upmu_get_chrdet_ana_major_rev(
	void);
extern unsigned int mt6358_upmu_get_chrdet_dig_minor_rev(
	void);
extern unsigned int mt6358_upmu_get_chrdet_dig_major_rev(
	void);
extern unsigned int mt6358_upmu_get_chrdet_cbs(
	void);
extern unsigned int mt6358_upmu_get_chrdet_bix(
	void);
extern unsigned int mt6358_upmu_get_chrdet_fpi(
	void);
extern unsigned int mt6358_upmu_set_rg_vcdt_rst_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vcdt_hv_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vcdt_hv_en(
	void);
extern unsigned int mt6358_upmu_set_rg_vcdt_deb_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_pchr_ft_ctrl(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rgs_chrdet(
	void);
extern unsigned int mt6358_upmu_get_rgs_vcdt_lv_det(
	void);
extern unsigned int mt6358_upmu_get_rgs_vcdt_hv_det(
	void);
extern unsigned int mt6358_upmu_set_rg_vcdt_lv_vth(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vcdt_hv_vth(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rgs_pchr_flag_out(
	void);
extern unsigned int mt6358_upmu_set_rg_pchr_flag_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_pchr_flag_en(
	void);
extern unsigned int mt6358_upmu_set_rg_pchr_flag_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_envtem_d(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_envtem_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_envtem_en(
	void);
extern unsigned int mt6358_upmu_set_da_qi_bgr_ext_buf_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_bgr_test_rstb(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_bgr_test_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_bgr_test_en(
	void);
extern unsigned int mt6358_upmu_set_rg_bgr_unchop(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_bgr_unchop_ph(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_otg_bvalid_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_otg_bvalid_en(
	void);
extern unsigned int mt6358_upmu_set_rg_uvlo_vthl(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_lbat_int_vth(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_pchr_rv(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rgs_otg_bvalid_det(
	void);
extern unsigned int mt6358_upmu_get_rgs_chr_ldo_det(
	void);
extern unsigned int mt6358_upmu_set_rg_vcdt_uvlo_vth(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vcdt_trim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_bgr_trim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_bgr_rsel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_bgr_trim_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_bgr_trim_en(
	void);
extern unsigned int mt6358_upmu_set_rg_baton_32k_ck_pdn(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_baton_1k_ck_pdn(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_fgadc_ft_ck_pdn(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_fgadc_dig_ck_pdn(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_fgadc_ana_ck_pdn(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_g_bif_1m_ck_pdn(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_bif_x1_ck_pdn(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_bif_x4_ck_pdn(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_bif_x104_ck_pdn(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_bm_intrp_ck_pdn(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_bm_top_ckpdn_con0_rsv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_fgadc_ana_ck_cksel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_bif_x4_ck_divsel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_bm_top_ckdivsel_con0_rsv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_bif_x4_ck_pdn_hwen(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_bif_x104_ck_pdn_hwen(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_bm_intrp_ck_pdn_hwen(
	unsigned int val);
extern unsigned int mt6358_upmu_set_bm_top_ckhwen_con0_rsv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_fg_ck_tstsel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_fgadc_ana_ck_tstsel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_fg_ck_tst_dis(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_fgadc_swrst(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_baton_swrst(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_bif_swrst(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_bank_fgadc_ana_swrst(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_bank_fgadc0_swrst(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_bank_fgadc1_swrst(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_bank_baton_ana_swrst(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_bank_baton_swrst(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_bank_bif_swrst(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_fgadc_rst_src_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_bm_top_rst_con1_rsv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_en_fg_bat0_h(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_fg_bat0_h(
	void);
extern unsigned int mt6358_upmu_set_rg_int_en_fg_bat0_l(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_fg_bat0_l(
	void);
extern unsigned int mt6358_upmu_set_rg_int_en_fg_cur_h(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_fg_cur_h(
	void);
extern unsigned int mt6358_upmu_set_rg_int_en_fg_cur_l(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_fg_cur_l(
	void);
extern unsigned int mt6358_upmu_set_rg_int_en_fg_zcv(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_fg_zcv(
	void);
extern unsigned int mt6358_upmu_set_rg_int_en_fg_bat1_h(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_fg_bat1_h(
	void);
extern unsigned int mt6358_upmu_set_rg_int_en_fg_bat1_l(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_fg_bat1_l(
	void);
extern unsigned int mt6358_upmu_set_rg_int_en_fg_n_charge_l(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_fg_n_charge_l(
	void);
extern unsigned int mt6358_upmu_set_rg_int_en_fg_iavg_h(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_fg_iavg_h(
	void);
extern unsigned int mt6358_upmu_set_rg_int_en_fg_iavg_l(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_fg_iavg_l(
	void);
extern unsigned int mt6358_upmu_set_rg_int_en_fg_time_h(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_fg_time_h(
	void);
extern unsigned int mt6358_upmu_set_rg_int_en_fg_discharge(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_fg_discharge(
	void);
extern unsigned int mt6358_upmu_set_rg_int_en_fg_charge(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_fg_charge(
	void);
extern unsigned int mt6358_upmu_set_rg_bm_int_en_con0_rsv(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_bm_int_en_con0_rsv(
	void);
extern unsigned int mt6358_upmu_set_rg_int_en_baton_lv(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_baton_lv(
	void);
extern unsigned int mt6358_upmu_set_rg_int_en_baton_ht(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_baton_ht(
	void);
extern unsigned int mt6358_upmu_set_rg_int_en_baton_bat_in(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_baton_bat_in(
	void);
extern unsigned int mt6358_upmu_set_rg_int_en_baton_bat_out(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_baton_bat_out(
	void);
extern unsigned int mt6358_upmu_set_rg_int_en_bif(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_bif(
	void);
extern unsigned int mt6358_upmu_set_rg_int_mask_fg_bat0_h(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_fg_bat0_l(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_fg_cur_h(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_fg_cur_l(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_fg_zcv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_fg_bat1_h(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_fg_bat1_l(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_fg_n_charge_l(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_fg_iavg_h(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_fg_iavg_l(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_fg_time_h(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_fg_discharge(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_fg_charge(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_bm_int_mask_con0_rsv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_baton_lv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_baton_ht(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_baton_bat_in(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_baton_bat_out(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_bif(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_fg_bat0_h(
	void);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_fg_bat0_l(
	void);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_fg_cur_h(
	void);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_fg_cur_l(
	void);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_fg_zcv(
	void);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_fg_bat1_h(
	void);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_fg_bat1_l(
	void);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_fg_n_charge_l(
	void);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_fg_iavg_h(
	void);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_fg_iavg_l(
	void);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_fg_time_h(
	void);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_fg_discharge(
	void);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_fg_charge(
	void);
extern unsigned int mt6358_upmu_get_rg_bm_int_raw_status_rsv(
	void);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_baton_lv(
	void);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_baton_ht(
	void);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_baton_bat_in(
	void);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_baton_bat_out(
	void);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_bif(
	void);
extern unsigned int mt6358_upmu_set_polarity(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_bm_mon_flag_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_bm_mon_grp_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_bm_top_rsv0(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_fganalogtest(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_fgintmode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_spare(
	unsigned int val);
extern unsigned int mt6358_upmu_set_fg_rng_bit_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_fg_rng_bit_sw(
	unsigned int val);
extern unsigned int mt6358_upmu_set_fg_rng_en_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_fg_rng_en_sw(
	unsigned int val);
extern unsigned int mt6358_upmu_get_da_fg_rng_en(
	void);
extern unsigned int mt6358_upmu_get_da_fg_rng_bit(
	void);
extern unsigned int mt6358_upmu_set_fg_dwa_t0(
	unsigned int val);
extern unsigned int mt6358_upmu_set_fg_dwa_t1(
	unsigned int val);
extern unsigned int mt6358_upmu_set_fg_dwa_rst_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_fg_dwa_rst_sw(
	unsigned int val);
extern unsigned int mt6358_upmu_get_da_dwa_rst(
	void);
extern unsigned int mt6358_upmu_set_rg_fgadc_gainerror_cal(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_fg_offset_swap(
	unsigned int val);
extern unsigned int mt6358_upmu_set_fg_on(
	unsigned int val);
extern unsigned int mt6358_upmu_set_fg_cal(
	unsigned int val);
extern unsigned int mt6358_upmu_set_fg_autocalrate(
	unsigned int val);
extern unsigned int mt6358_upmu_set_fg_son_slp_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_fg_soff_slp_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_fg_zcv_det_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_fg_auxadc_r(
	unsigned int val);
extern unsigned int mt6358_upmu_set_fg_iavg_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_fg_sw_read_pre(
	unsigned int val);
extern unsigned int mt6358_upmu_set_fg_sw_rstclr(
	unsigned int val);
extern unsigned int mt6358_upmu_set_fg_sw_cr(
	unsigned int val);
extern unsigned int mt6358_upmu_set_fg_sw_clear(
	unsigned int val);
extern unsigned int mt6358_upmu_set_fg_offset_rst(
	unsigned int val);
extern unsigned int mt6358_upmu_set_fg_time_rst(
	unsigned int val);
extern unsigned int mt6358_upmu_set_fg_charge_rst(
	unsigned int val);
extern unsigned int mt6358_upmu_set_fg_n_charge_rst(
	unsigned int val);
extern unsigned int mt6358_upmu_set_fg_soff_rst(
	unsigned int val);
extern unsigned int mt6358_upmu_get_fg_latchdata_st(
	void);
extern unsigned int mt6358_upmu_get_event_fg_bat0_h(
	void);
extern unsigned int mt6358_upmu_get_event_fg_bat0_l(
	void);
extern unsigned int mt6358_upmu_get_event_fg_cur_h(
	void);
extern unsigned int mt6358_upmu_get_event_fg_cur_l(
	void);
extern unsigned int mt6358_upmu_get_event_fg_zcv(
	void);
extern unsigned int mt6358_upmu_get_event_fg_bat1_h(
	void);
extern unsigned int mt6358_upmu_get_event_fg_bat1_l(
	void);
extern unsigned int mt6358_upmu_get_event_fg_n_charge_l(
	void);
extern unsigned int mt6358_upmu_get_event_fg_iavg_h(
	void);
extern unsigned int mt6358_upmu_get_event_fg_iavg_l(
	void);
extern unsigned int mt6358_upmu_get_event_fg_time_h(
	void);
extern unsigned int mt6358_upmu_get_event_fg_discharge(
	void);
extern unsigned int mt6358_upmu_get_event_fg_charge(
	void);
extern unsigned int mt6358_upmu_set_fg_osr1(
	unsigned int val);
extern unsigned int mt6358_upmu_set_fg_osr2(
	unsigned int val);
extern unsigned int mt6358_upmu_set_fg_adj_offset_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_fg_adc_autorst(
	unsigned int val);
extern unsigned int mt6358_upmu_get_fg_adc_rstdetect(
	void);
extern unsigned int mt6358_upmu_set_fg_va_aon(
	unsigned int val);
extern unsigned int mt6358_upmu_set_fg_va_aoff(
	unsigned int val);
extern unsigned int mt6358_upmu_set_fg_son_sw(
	unsigned int val);
extern unsigned int mt6358_upmu_set_fg_son_sw_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_fgadc_con4_rsv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_fg_rstb_status(
	unsigned int val);
extern unsigned int mt6358_upmu_get_fg_car_15_00(
	void);
extern unsigned int mt6358_upmu_get_fg_car_31_16(
	void);
extern unsigned int mt6358_upmu_set_fg_bat0_lth_15_00(
	unsigned int val);
extern unsigned int mt6358_upmu_set_fg_bat0_lth_31_16(
	unsigned int val);
extern unsigned int mt6358_upmu_set_fg_bat0_hth_15_00(
	unsigned int val);
extern unsigned int mt6358_upmu_set_fg_bat0_hth_31_16(
	unsigned int val);
extern unsigned int mt6358_upmu_set_fg_bat1_lth_15_00(
	unsigned int val);
extern unsigned int mt6358_upmu_set_fg_bat1_lth_31_16(
	unsigned int val);
extern unsigned int mt6358_upmu_set_fg_bat1_hth_15_00(
	unsigned int val);
extern unsigned int mt6358_upmu_set_fg_bat1_hth_31_16(
	unsigned int val);
extern unsigned int mt6358_upmu_get_fg_ncar_15_00(
	void);
extern unsigned int mt6358_upmu_get_fg_ncar_31_16(
	void);
extern unsigned int mt6358_upmu_set_fg_n_charge_lth_15_00(
	unsigned int val);
extern unsigned int mt6358_upmu_set_fg_n_charge_lth_31_16(
	unsigned int val);
extern unsigned int mt6358_upmu_get_fg_iavg_15_00(
	void);
extern unsigned int mt6358_upmu_get_fg_iavg_27_16(
	void);
extern unsigned int mt6358_upmu_get_fg_iavg_vld(
	void);
extern unsigned int mt6358_upmu_set_fg_iavg_lth_15_00(
	unsigned int val);
extern unsigned int mt6358_upmu_set_fg_iavg_lth_28_16(
	unsigned int val);
extern unsigned int mt6358_upmu_set_fg_iavg_hth_15_00(
	unsigned int val);
extern unsigned int mt6358_upmu_set_fg_iavg_hth_28_16(
	unsigned int val);
extern unsigned int mt6358_upmu_get_fg_nter_15_00(
	void);
extern unsigned int mt6358_upmu_get_fg_nter_29_16(
	void);
extern unsigned int mt6358_upmu_set_fg_time_hth_15_00(
	unsigned int val);
extern unsigned int mt6358_upmu_set_fg_time_hth_29_16(
	unsigned int val);
extern unsigned int mt6358_upmu_set_fg_son_slp_cur_th(
	unsigned int val);
extern unsigned int mt6358_upmu_set_fg_son_slp_time(
	unsigned int val);
extern unsigned int mt6358_upmu_set_fg_son_det_time(
	unsigned int val);
extern unsigned int mt6358_upmu_set_fg_fp_ftime(
	unsigned int val);
extern unsigned int mt6358_upmu_set_fg_soff_slp_cur_th(
	unsigned int val);
extern unsigned int mt6358_upmu_set_fg_soff_slp_time(
	unsigned int val);
extern unsigned int mt6358_upmu_set_fg_soff_det_time(
	unsigned int val);
extern unsigned int mt6358_upmu_get_fg_soff_time_15_00(
	void);
extern unsigned int mt6358_upmu_get_fg_soff_time_29_16(
	void);
extern unsigned int mt6358_upmu_get_fg_soff(
	void);
extern unsigned int mt6358_upmu_set_fg_pwr_time0(
	unsigned int val);
extern unsigned int mt6358_upmu_set_fg_pwr_time1(
	unsigned int val);
extern unsigned int mt6358_upmu_set_fg_pwr_time2(
	unsigned int val);
extern unsigned int mt6358_upmu_set_fg_zcv_det_iv(
	unsigned int val);
extern unsigned int mt6358_upmu_get_fg_zcv_curr(
	void);
extern unsigned int mt6358_upmu_get_fg_zcv_car_15_00(
	void);
extern unsigned int mt6358_upmu_get_fg_zcv_car_31_16(
	void);
extern unsigned int mt6358_upmu_set_fg_zcv_car_th_15_00(
	unsigned int val);
extern unsigned int mt6358_upmu_set_fg_zcv_car_th_30_16(
	unsigned int val);
extern unsigned int mt6358_upmu_get_fg_r_curr(
	void);
extern unsigned int mt6358_upmu_get_fg_current_out(
	void);
extern unsigned int mt6358_upmu_set_fg_cur_lth(
	unsigned int val);
extern unsigned int mt6358_upmu_set_fg_cur_hth(
	unsigned int val);
extern unsigned int mt6358_upmu_get_fg_cic2(
	void);
extern unsigned int mt6358_upmu_get_fg_offset(
	void);
extern unsigned int mt6358_upmu_set_fg_adjust_offset_value(
	unsigned int val);
extern unsigned int mt6358_upmu_set_fg_gain(
	unsigned int val);
extern unsigned int mt6358_upmu_set_fg_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_fg_rst_sw(
	unsigned int val);
extern unsigned int mt6358_upmu_set_fg_fgcal_en_sw(
	unsigned int val);
extern unsigned int mt6358_upmu_set_fg_fgadc_en_sw(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_system_info_con0(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_baton_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_baton_en(
	void);
extern unsigned int mt6358_upmu_set_rg_bif_batid_sw_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_bif_batid_sw_en(
	void);
extern unsigned int mt6358_upmu_set_rg_qi_baton_lt_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_qi_baton_lt_en(
	void);
extern unsigned int mt6358_upmu_set_rg_baton_ht_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_baton_ht_en(
	void);
extern unsigned int mt6358_upmu_set_rg_baton_ht_vth(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_hw_vth1(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_hw_vth2(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_hw_vth_ctrl(
	unsigned int val);
extern unsigned int mt6358_upmu_get_da_baton_ht_en(
	void);
extern unsigned int mt6358_upmu_get_ad_baton_undet(
	void);
extern unsigned int mt6358_upmu_get_ad_baton_undet_raw(
	void);
extern unsigned int mt6358_upmu_get_ad_baton_hv(
	void);
extern unsigned int mt6358_upmu_get_da_bif_tx_en(
	void);
extern unsigned int mt6358_upmu_get_da_bif_tx_data(
	void);
extern unsigned int mt6358_upmu_get_da_bif_rx_en(
	void);
extern unsigned int mt6358_upmu_get_ad_bif_rx_data(
	void);
extern unsigned int mt6358_upmu_set_rg_baton_ht_trim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_baton_debounce_wnd(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_baton_debounce_thd(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_baton_chrdet_test_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_baton_undet_test_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_baton_auxadc_test_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_baton_fgadc_test_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_baton_rtc_test_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_baton_bif_test_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_baton_rsv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_baton_chrdet_sw(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_baton_undet_sw(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_baton_auxadc_sw(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_baton_fgadc_sw(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_baton_rtc_sw(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_baton_bif_sw(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_baton_rsv_sw(
	unsigned int val);
extern unsigned int mt6358_upmu_get_baton_status(
	void);
extern unsigned int mt6358_upmu_get_baton_auxadc_set(
	void);
extern unsigned int mt6358_upmu_get_baton_deb_valid(
	void);
extern unsigned int mt6358_upmu_get_baton_bif_status(
	void);
extern unsigned int mt6358_upmu_get_baton_rtc_status(
	void);
extern unsigned int mt6358_upmu_get_baton_fgadc_status(
	void);
extern unsigned int mt6358_upmu_get_baton_auxadc_trig(
	void);
extern unsigned int mt6358_upmu_get_baton_chrdet_deb(
	void);
extern unsigned int mt6358_upmu_get_baton_ivgen_enb(
	void);
extern unsigned int mt6358_upmu_get_baton_vbif28_stb(
	void);
extern unsigned int mt6358_upmu_get_baton_vbif28_en(
	void);
extern unsigned int mt6358_upmu_get_baton_rsv_0(
	void);
extern unsigned int mt6358_upmu_set_rg_baton_pwr_time0(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_baton_pwr_time1(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_baton_chrdet_deb_td(
	unsigned int val);
extern unsigned int mt6358_upmu_set_bif_command_0(
	unsigned int val);
extern unsigned int mt6358_upmu_set_bif_command_1(
	unsigned int val);
extern unsigned int mt6358_upmu_set_bif_command_2(
	unsigned int val);
extern unsigned int mt6358_upmu_set_bif_command_3(
	unsigned int val);
extern unsigned int mt6358_upmu_set_bif_command_4(
	unsigned int val);
extern unsigned int mt6358_upmu_set_bif_command_5(
	unsigned int val);
extern unsigned int mt6358_upmu_set_bif_command_6(
	unsigned int val);
extern unsigned int mt6358_upmu_set_bif_command_7(
	unsigned int val);
extern unsigned int mt6358_upmu_set_bif_command_8(
	unsigned int val);
extern unsigned int mt6358_upmu_set_bif_command_9(
	unsigned int val);
extern unsigned int mt6358_upmu_set_bif_command_10(
	unsigned int val);
extern unsigned int mt6358_upmu_set_bif_command_11(
	unsigned int val);
extern unsigned int mt6358_upmu_set_bif_command_12(
	unsigned int val);
extern unsigned int mt6358_upmu_set_bif_command_13(
	unsigned int val);
extern unsigned int mt6358_upmu_set_bif_command_14(
	unsigned int val);
extern unsigned int mt6358_upmu_set_bif_rsv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_bif_command_type(
	unsigned int val);
extern unsigned int mt6358_upmu_set_bif_trasfer_num(
	unsigned int val);
extern unsigned int mt6358_upmu_set_bif_logic_0_set(
	unsigned int val);
extern unsigned int mt6358_upmu_set_bif_logic_1_set(
	unsigned int val);
extern unsigned int mt6358_upmu_set_bif_stop_set(
	unsigned int val);
extern unsigned int mt6358_upmu_set_bif_debounce_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_bif_read_expect_num(
	unsigned int val);
extern unsigned int mt6358_upmu_set_bif_trasact_trigger(
	unsigned int val);
extern unsigned int mt6358_upmu_get_bif_data_num(
	void);
extern unsigned int mt6358_upmu_get_bif_response(
	void);
extern unsigned int mt6358_upmu_get_bif_data_0(
	void);
extern unsigned int mt6358_upmu_get_bif_ack_0(
	void);
extern unsigned int mt6358_upmu_get_bif_error_0(
	void);
extern unsigned int mt6358_upmu_get_bif_data_1(
	void);
extern unsigned int mt6358_upmu_get_bif_ack_1(
	void);
extern unsigned int mt6358_upmu_get_bif_error_1(
	void);
extern unsigned int mt6358_upmu_get_bif_data_2(
	void);
extern unsigned int mt6358_upmu_get_bif_ack_2(
	void);
extern unsigned int mt6358_upmu_get_bif_error_2(
	void);
extern unsigned int mt6358_upmu_get_bif_data_3(
	void);
extern unsigned int mt6358_upmu_get_bif_ack_3(
	void);
extern unsigned int mt6358_upmu_get_bif_error_3(
	void);
extern unsigned int mt6358_upmu_get_bif_data_4(
	void);
extern unsigned int mt6358_upmu_get_bif_ack_4(
	void);
extern unsigned int mt6358_upmu_get_bif_error_4(
	void);
extern unsigned int mt6358_upmu_get_bif_data_5(
	void);
extern unsigned int mt6358_upmu_get_bif_ack_5(
	void);
extern unsigned int mt6358_upmu_get_bif_error_5(
	void);
extern unsigned int mt6358_upmu_get_bif_data_6(
	void);
extern unsigned int mt6358_upmu_get_bif_ack_6(
	void);
extern unsigned int mt6358_upmu_get_bif_error_6(
	void);
extern unsigned int mt6358_upmu_get_bif_data_7(
	void);
extern unsigned int mt6358_upmu_get_bif_ack_7(
	void);
extern unsigned int mt6358_upmu_get_bif_error_7(
	void);
extern unsigned int mt6358_upmu_get_bif_data_8(
	void);
extern unsigned int mt6358_upmu_get_bif_ack_8(
	void);
extern unsigned int mt6358_upmu_get_bif_error_8(
	void);
extern unsigned int mt6358_upmu_get_bif_data_9(
	void);
extern unsigned int mt6358_upmu_get_bif_ack_9(
	void);
extern unsigned int mt6358_upmu_get_bif_error_9(
	void);
extern unsigned int mt6358_upmu_set_bif_test_mode0(
	unsigned int val);
extern unsigned int mt6358_upmu_set_bif_test_mode1(
	unsigned int val);
extern unsigned int mt6358_upmu_set_bif_test_mode2(
	unsigned int val);
extern unsigned int mt6358_upmu_set_bif_test_mode3(
	unsigned int val);
extern unsigned int mt6358_upmu_set_bif_test_mode4(
	unsigned int val);
extern unsigned int mt6358_upmu_set_bif_test_mode5(
	unsigned int val);
extern unsigned int mt6358_upmu_set_bif_test_mode6(
	unsigned int val);
extern unsigned int mt6358_upmu_set_bif_test_mode7(
	unsigned int val);
extern unsigned int mt6358_upmu_set_bif_test_mode8(
	unsigned int val);
extern unsigned int mt6358_upmu_set_bif_bat_lost_sw(
	unsigned int val);
extern unsigned int mt6358_upmu_set_bif_rx_data_sw(
	unsigned int val);
extern unsigned int mt6358_upmu_set_bif_tx_data_sw(
	unsigned int val);
extern unsigned int mt6358_upmu_set_bif_rx_en_sw(
	unsigned int val);
extern unsigned int mt6358_upmu_set_bif_tx_en_sw(
	unsigned int val);
extern unsigned int mt6358_upmu_set_bif_back_normal(
	unsigned int val);
extern unsigned int mt6358_upmu_set_bif_irq_clr(
	unsigned int val);
extern unsigned int mt6358_upmu_get_bif_irq(
	void);
extern unsigned int mt6358_upmu_get_bif_timeout(
	void);
extern unsigned int mt6358_upmu_get_bif_bat_undet(
	void);
extern unsigned int mt6358_upmu_get_bif_total_valid(
	void);
extern unsigned int mt6358_upmu_get_bif_bus_status(
	void);
extern unsigned int mt6358_upmu_set_bif_power_up_count(
	unsigned int val);
extern unsigned int mt6358_upmu_set_bif_power_up(
	unsigned int val);
extern unsigned int mt6358_upmu_get_bif_rx_error_unknown(
	void);
extern unsigned int mt6358_upmu_get_bif_rx_error_insuff(
	void);
extern unsigned int mt6358_upmu_get_bif_rx_error_lowphase(
	void);
extern unsigned int mt6358_upmu_get_bif_rx_state(
	void);
extern unsigned int mt6358_upmu_get_bif_flow_ctl_state(
	void);
extern unsigned int mt6358_upmu_get_bif_tx_state(
	void);
extern unsigned int mt6358_upmu_get_bif_tx_data_fianl(
	void);
extern unsigned int mt6358_upmu_get_bif_rx_data_sampling(
	void);
extern unsigned int mt6358_upmu_get_bif_rx_data_recovery(
	void);
extern unsigned int mt6358_upmu_set_rg_baton_ht_en_pre(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_baton_ht_en_pre(
	void);
extern unsigned int mt6358_upmu_set_rg_baton_ht_en_dly_time(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_baton_ht_en_dly_time(
	void);
extern unsigned int mt6358_upmu_set_bif_timeout_set(
	unsigned int val);
extern unsigned int mt6358_upmu_set_bif_rx_deg_wnd(
	unsigned int val);
extern unsigned int mt6358_upmu_set_bif_rx_deg_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_bif_rsv1(
	void);
extern unsigned int mt6358_upmu_set_bif_rsv0(
	unsigned int val);
extern unsigned int mt6358_upmu_get_hk_top_ana_id(
	void);
extern unsigned int mt6358_upmu_get_hk_top_dig_id(
	void);
extern unsigned int mt6358_upmu_get_hk_top_ana_minor_rev(
	void);
extern unsigned int mt6358_upmu_get_hk_top_ana_major_rev(
	void);
extern unsigned int mt6358_upmu_get_hk_top_dig_minor_rev(
	void);
extern unsigned int mt6358_upmu_get_hk_top_dig_major_rev(
	void);
extern unsigned int mt6358_upmu_get_hk_top_cbs(
	void);
extern unsigned int mt6358_upmu_get_hk_top_bix(
	void);
extern unsigned int mt6358_upmu_get_hk_top_esp(
	void);
extern unsigned int mt6358_upmu_get_hk_top_fpi(
	void);
extern unsigned int mt6358_upmu_get_hk_clk_offset(
	void);
extern unsigned int mt6358_upmu_get_hk_rst_offset(
	void);
extern unsigned int mt6358_upmu_get_hk_int_offset(
	void);
extern unsigned int mt6358_upmu_get_hk_int_len(
	void);
extern unsigned int mt6358_upmu_set_rg_auxadc_ao_1m_ck_pdn(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_auxadc_1m_ck_pdn_hwen(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_auxadc_1m_ck_pdn(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_auxadc_ck_pdn_hwen(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_auxadc_ck_pdn(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_auxadc_rng_ck_pdn_hwen(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_auxadc_rng_ck_pdn(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_auxadc_32k_ck_pdn_hwen(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_auxadc_32k_ck_pdn(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_auxadc_1k_ck_pdn_hwen(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_auxadc_1k_ck_pdn(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_hk_intrp_ck_pdn_hwen(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_hk_intrp_ck_pdn(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_auxadc_ck_divsel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_auxadc_ck_tstsel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_auxadc_rst(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_auxadc_reg_rst(
	unsigned int val);
extern unsigned int mt6358_upmu_set_bank_hk_top_swrst(
	unsigned int val);
extern unsigned int mt6358_upmu_set_bank_auxadc_swrst(
	unsigned int val);
extern unsigned int mt6358_upmu_set_bank_auxadc_dig_1_swrst(
	unsigned int val);
extern unsigned int mt6358_upmu_set_bank_auxadc_dig_2_swrst(
	unsigned int val);
extern unsigned int mt6358_upmu_set_bank_auxadc_dig_3_swrst(
	unsigned int val);
extern unsigned int mt6358_upmu_set_bank_auxadc_dig_4_swrst(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_en_bat_h(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_bat_h(
	void);
extern unsigned int mt6358_upmu_set_rg_int_en_bat_l(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_bat_l(
	void);
extern unsigned int mt6358_upmu_set_rg_int_en_bat2_h(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_bat2_h(
	void);
extern unsigned int mt6358_upmu_set_rg_int_en_bat2_l(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_bat2_l(
	void);
extern unsigned int mt6358_upmu_set_rg_int_en_bat_temp_h(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_bat_temp_h(
	void);
extern unsigned int mt6358_upmu_set_rg_int_en_bat_temp_l(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_bat_temp_l(
	void);
extern unsigned int mt6358_upmu_set_rg_int_en_auxadc_imp(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_auxadc_imp(
	void);
extern unsigned int mt6358_upmu_set_rg_int_en_nag_c_dltv(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_nag_c_dltv(
	void);
extern unsigned int mt6358_upmu_set_rg_int_mask_bat_h(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_bat_l(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_bat2_h(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_bat2_l(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_bat_temp_h(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_bat_temp_l(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_auxadc_imp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_nag_c_dltv(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_bat_h(
	void);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_bat_l(
	void);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_bat2_h(
	void);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_bat2_l(
	void);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_bat_temp_h(
	void);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_bat_temp_l(
	void);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_auxadc_imp(
	void);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_nag_c_dltv(
	void);
extern unsigned int mt6358_upmu_set_rg_clk_mon_flag_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_clk_mon_flag_en(
	void);
extern unsigned int mt6358_upmu_set_rg_clk_mon_flag_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mon_flag_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_mon_flag_en(
	void);
extern unsigned int mt6358_upmu_set_rg_int_mon_flag_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_hk_mon_flag_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_mon_flag_sel_auxadc(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_adcin_vsen_mux_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_adcin_vsen_mux_en(
	void);
extern unsigned int mt6358_upmu_set_rg_baton_tdet_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_baton_tdet_en(
	void);
extern unsigned int mt6358_upmu_set_rg_adcin_vsen_ext_baton_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_adcin_vsen_ext_baton_en(
	void);
extern unsigned int mt6358_upmu_set_rg_adcin_vbat_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_adcin_vbat_en(
	void);
extern unsigned int mt6358_upmu_set_rg_adcin_vsen_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_adcin_vsen_en(
	void);
extern unsigned int mt6358_upmu_set_rg_adcin_chr_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_adcin_chr_en(
	void);
extern unsigned int mt6358_upmu_get_da_adcin_vbat_en(
	void);
extern unsigned int mt6358_upmu_get_da_auxadc_vbat_en(
	void);
extern unsigned int mt6358_upmu_get_da_adcin_vsen_mux_en(
	void);
extern unsigned int mt6358_upmu_get_da_adcin_vsen_en(
	void);
extern unsigned int mt6358_upmu_get_da_adcin_chr_en(
	void);
extern unsigned int mt6358_upmu_get_da_baton_tdet_en(
	void);
extern unsigned int mt6358_upmu_get_da_adcin_batid_sw_en(
	void);
extern unsigned int mt6358_upmu_get_auxadc_ana_id(
	void);
extern unsigned int mt6358_upmu_get_auxadc_dig_id(
	void);
extern unsigned int mt6358_upmu_get_auxadc_ana_minor_rev(
	void);
extern unsigned int mt6358_upmu_get_auxadc_ana_major_rev(
	void);
extern unsigned int mt6358_upmu_get_auxadc_dig_minor_rev(
	void);
extern unsigned int mt6358_upmu_get_auxadc_dig_major_rev(
	void);
extern unsigned int mt6358_upmu_get_auxadc_dsn_cbs(
	void);
extern unsigned int mt6358_upmu_get_auxadc_dsn_bix(
	void);
extern unsigned int mt6358_upmu_set_rg_auxadc_cali(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_aux_rsv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vbuf_byp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vbuf_calen(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vbuf_exten(
	unsigned int val);
extern unsigned int mt6358_upmu_get_auxadc_dig_1_ana_id(
	void);
extern unsigned int mt6358_upmu_get_auxadc_dig_1_dig_id(
	void);
extern unsigned int mt6358_upmu_get_auxadc_dig_1_ana_minor_rev(
	void);
extern unsigned int mt6358_upmu_get_auxadc_dig_1_ana_major_rev(
	void);
extern unsigned int mt6358_upmu_get_auxadc_dig_1_dig_minor_rev(
	void);
extern unsigned int mt6358_upmu_get_auxadc_dig_1_dig_major_rev(
	void);
extern unsigned int mt6358_upmu_get_auxadc_dig_1_dsn_cbs(
	void);
extern unsigned int mt6358_upmu_get_auxadc_dig_1_dsn_bix(
	void);
extern unsigned int mt6358_upmu_get_auxadc_dig_1_dsn_esp(
	void);
extern unsigned int mt6358_upmu_get_auxadc_dig_1_dsn_fpi(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_out_ch0(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_rdy_ch0(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_out_ch1(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_rdy_ch1(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_out_ch2(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_rdy_ch2(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_out_ch3(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_rdy_ch3(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_out_ch4(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_rdy_ch4(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_out_ch5(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_rdy_ch5(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_out_ch6(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_rdy_ch6(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_out_ch7(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_rdy_ch7(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_out_ch8(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_rdy_ch8(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_out_ch9(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_rdy_ch9(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_out_ch10(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_rdy_ch10(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_out_ch11(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_rdy_ch11(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_out_ch12_15(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_rdy_ch12_15(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_out_lbat(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_rdy_lbat(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_out_lbat2(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_rdy_lbat2(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_out_ch7_by_gps(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_rdy_ch7_by_gps(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_out_ch7_by_md(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_rdy_ch7_by_md(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_out_ch7_by_ap(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_rdy_ch7_by_ap(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_out_ch4_by_md(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_rdy_ch4_by_md(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_out_pwron_pchr(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_rdy_pwron_pchr(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_out_wakeup_pchr(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_rdy_wakeup_pchr(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_out_ch0_by_md(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_rdy_ch0_by_md(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_out_ch0_by_ap(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_rdy_ch0_by_ap(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_out_ch1_by_md(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_rdy_ch1_by_md(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_out_ch1_by_ap(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_rdy_ch1_by_ap(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_out_bat_temp(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_rdy_bat_temp(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_out_fgadc_pchr(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_rdy_fgadc_pchr(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_out_bat_plugin_pchr(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_rdy_bat_plugin_pchr(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_out_imp(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_rdy_imp(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_out_imp_avg(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_rdy_imp_avg(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_out_raw(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_out_mdrt(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_rdy_mdrt(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_out_dcxo_by_gps(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_rdy_dcxo_by_gps(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_out_dcxo_by_md(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_rdy_dcxo_by_md(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_out_dcxo_by_ap(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_rdy_dcxo_by_ap(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_out_dcxo_mdrt(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_rdy_dcxo_mdrt(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_out_nag(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_rdy_nag(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_out_batid(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_rdy_batid(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_out_ch4_by_thr1(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_rdy_ch4_by_thr1(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_out_ch4_by_thr2(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_rdy_ch4_by_thr2(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_out_ch4_by_thr3(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_rdy_ch4_by_thr3(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_busy_in(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_busy_in_lbat(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_busy_in_lbat2(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_busy_in_bat_temp(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_busy_in_wakeup(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_busy_in_dcxo_mdrt(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_busy_in_dcxo_gps_ap(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_busy_in_dcxo_gps_md(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_busy_in_dcxo_gps(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_busy_in_mdrt(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_busy_in_share(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_busy_in_imp(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_busy_in_fgadc_pchr(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_busy_in_gps_ap(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_busy_in_gps_md(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_busy_in_gps(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_busy_in_thr_md(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_busy_in_bat_plugin_pchr(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_busy_in_batid(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_busy_in_pwron(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_busy_in_thr1(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_busy_in_thr2(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_busy_in_thr3(
	void);
extern unsigned int mt6358_upmu_get_auxadc_adc_busy_in_nag(
	void);
extern unsigned int mt6358_upmu_get_auxadc_dig_2_ana_id(
	void);
extern unsigned int mt6358_upmu_get_auxadc_dig_2_dig_id(
	void);
extern unsigned int mt6358_upmu_get_auxadc_dig_2_ana_minor_rev(
	void);
extern unsigned int mt6358_upmu_get_auxadc_dig_2_ana_major_rev(
	void);
extern unsigned int mt6358_upmu_get_auxadc_dig_2_dig_minor_rev(
	void);
extern unsigned int mt6358_upmu_get_auxadc_dig_2_dig_major_rev(
	void);
extern unsigned int mt6358_upmu_get_auxadc_dig_2_dsn_cbs(
	void);
extern unsigned int mt6358_upmu_get_auxadc_dig_2_dsn_bix(
	void);
extern unsigned int mt6358_upmu_get_auxadc_dig_2_dsn_esp(
	void);
extern unsigned int mt6358_upmu_get_auxadc_dig_2_dsn_fpi(
	void);
extern unsigned int mt6358_upmu_get_auxadc_dig_3_ana_id(
	void);
extern unsigned int mt6358_upmu_get_auxadc_dig_3_dig_id(
	void);
extern unsigned int mt6358_upmu_get_auxadc_dig_3_ana_minor_rev(
	void);
extern unsigned int mt6358_upmu_get_auxadc_dig_3_ana_major_rev(
	void);
extern unsigned int mt6358_upmu_get_auxadc_dig_3_dig_minor_rev(
	void);
extern unsigned int mt6358_upmu_get_auxadc_dig_3_dig_major_rev(
	void);
extern unsigned int mt6358_upmu_get_auxadc_dig_3_dsn_cbs(
	void);
extern unsigned int mt6358_upmu_get_auxadc_dig_3_dsn_bix(
	void);
extern unsigned int mt6358_upmu_get_auxadc_dig_3_dsn_fpi(
	void);
extern unsigned int mt6358_upmu_set_auxadc_ck_on_extd(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_srclken_src_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_adc_pwdb(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_adc_pwdb_swctrl(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_strup_ck_on_enb(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_srclken_ck_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_ck_aon_gps(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_ck_aon_md(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_ck_aon(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_avg_num_small(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_avg_num_large(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_spl_num(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_avg_num_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_avg_num_sel_share(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_avg_num_sel_lbat(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_avg_num_sel_bat_temp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_avg_num_sel_wakeup(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_spl_num_large(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_spl_num_sleep(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_spl_num_sleep_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_spl_num_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_spl_num_sel_share(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_spl_num_sel_lbat(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_spl_num_sel_bat_temp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_spl_num_sel_wakeup(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_spl_num_ch0(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_spl_num_ch3(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_spl_num_ch7(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_avg_num_lbat(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_avg_num_ch7(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_avg_num_ch3(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_avg_num_ch0(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_avg_num_hpc(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_avg_num_dcxo(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_trim_ch0_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_trim_ch1_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_trim_ch2_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_trim_ch3_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_trim_ch4_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_trim_ch5_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_trim_ch6_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_trim_ch7_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_trim_ch8_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_trim_ch9_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_trim_ch10_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_trim_ch11_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_adc_2s_comp_enb(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_adc_trim_comp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_rng_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_test_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_bit_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_start_sw(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_start_swctrl(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_ts_vbe_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_ts_vbe_sel_swctrl(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_vbuf_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_vbuf_en_swctrl(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_out_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_da_dac(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_da_dac_swctrl(
	unsigned int val);
extern unsigned int mt6358_upmu_get_ad_auxadc_comp(
	void);
extern unsigned int mt6358_upmu_set_auxadc_adcin_vsen_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_adcin_vbat_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_adcin_vsen_mux_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_adcin_vsen_ext_baton_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_adcin_chr_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_adcin_baton_tdet_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_accdet_anaswctrl_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_xo_thadc_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_adcin_batid_sw_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_dig0_rsv0(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_chsel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_swctrl_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_source_lbat_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_source_lbat2_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_start_extd(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_dac_extd(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_dac_extd_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_dig0_rsv1(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_start_shade_num(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_start_shade_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_start_shade_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_adc_rdy_wakeup_clr(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_adc_rdy_fgadc_clr(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_adc_rdy_bat_plugin_clr(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_adc_rdy_pwron_clr(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_data_reuse_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_ch0_data_reuse_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_ch1_data_reuse_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_dcxo_data_reuse_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_data_reuse_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_ch0_data_reuse_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_ch1_data_reuse_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_dcxo_data_reuse_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_autorpt_prd(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_autorpt_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_accdet_auto_spl(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_accdet_auto_rqst_clr(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_accdet_dig1_rsv0(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_accdet_dig0_rsv0(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_fgadc_start_sw(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_fgadc_start_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_fgadc_r_sw(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_fgadc_r_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_bat_plugin_start_sw(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_bat_plugin_start_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_dbg_dig0_rsv2(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_dbg_dig1_rsv2(
	unsigned int val);
extern unsigned int mt6358_upmu_set_efuse_gain_ch7_trim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_efuse_offset_ch7_trim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_efuse_gain_ch4_trim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_efuse_offset_ch4_trim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_efuse_gain_ch0_trim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_efuse_offset_ch0_trim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_sw_gain_trim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_sw_offset_trim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_efuse_degc_cali(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_efuse_adc_cali_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_efuse_1rsv0(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_efuse_o_vts(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_efuse_2rsv0(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_efuse_o_slope(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_efuse_o_slope_sign(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_efuse_3rsv0(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_efuse_auxadc_rsv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_efuse_id(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_efuse_4rsv0(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_efuse_o_vts_2(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_efuse_2rsv0_2(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_efuse_o_vts_3(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_efuse_2rsv0_3(
	unsigned int val);
extern unsigned int mt6358_upmu_get_auxadc_dig_4_ana_id(
	void);
extern unsigned int mt6358_upmu_get_auxadc_dig_4_dig_id(
	void);
extern unsigned int mt6358_upmu_get_auxadc_dig_4_ana_minor_rev(
	void);
extern unsigned int mt6358_upmu_get_auxadc_dig_4_ana_major_rev(
	void);
extern unsigned int mt6358_upmu_get_auxadc_dig_4_dig_minor_rev(
	void);
extern unsigned int mt6358_upmu_get_auxadc_dig_4_dig_major_rev(
	void);
extern unsigned int mt6358_upmu_get_auxadc_dig_4_dsn_cbs(
	void);
extern unsigned int mt6358_upmu_get_auxadc_dig_4_dsn_bix(
	void);
extern unsigned int mt6358_upmu_get_auxadc_dig_4_dsn_esp(
	void);
extern unsigned int mt6358_upmu_get_auxadc_dig_4_dsn_fpi(
	void);
extern unsigned int mt6358_upmu_set_auxadc_impedance_cnt(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_impedance_chsel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_impedance_irq_clr(
	unsigned int val);
extern unsigned int mt6358_upmu_get_auxadc_impedance_irq_status(
	void);
extern unsigned int mt6358_upmu_set_auxadc_clr_imp_cnt_stop(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_impedance_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_imp_autorpt_prd(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_imp_autorpt_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_auxadc_imp_cnt(
	void);
extern unsigned int mt6358_upmu_get_auxadc_imp_cnt_stop_state(
	void);
extern unsigned int mt6358_upmu_set_auxadc_lbat_debt_max(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_lbat_debt_min(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_lbat_det_prd_15_0(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_lbat_det_prd_19_16(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_lbat_volt_max(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_lbat_irq_en_max(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_lbat_en_max(
	unsigned int val);
extern unsigned int mt6358_upmu_get_auxadc_lbat_max_irq_b(
	void);
extern unsigned int mt6358_upmu_set_auxadc_lbat_volt_min(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_lbat_irq_en_min(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_lbat_en_min(
	unsigned int val);
extern unsigned int mt6358_upmu_get_auxadc_lbat_min_irq_b(
	void);
extern unsigned int mt6358_upmu_get_auxadc_lbat_debounce_count_max(
	void);
extern unsigned int mt6358_upmu_get_auxadc_lbat_debounce_count_min(
	void);
extern unsigned int mt6358_upmu_set_auxadc_bat_temp_froze_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_bat_temp_debt_max(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_bat_temp_debt_min(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_bat_temp_det_prd_15_0(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_bat_temp_det_prd_19_16(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_bat_temp_volt_max(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_bat_temp_irq_en_max(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_bat_temp_en_max(
	unsigned int val);
extern unsigned int mt6358_upmu_get_auxadc_bat_temp_max_irq_b(
	void);
extern unsigned int mt6358_upmu_set_auxadc_bat_temp_volt_min(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_bat_temp_irq_en_min(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_bat_temp_en_min(
	unsigned int val);
extern unsigned int mt6358_upmu_get_auxadc_bat_temp_min_irq_b(
	void);
extern unsigned int mt6358_upmu_get_auxadc_bat_temp_debounce_count_max(
	void);
extern unsigned int mt6358_upmu_get_auxadc_bat_temp_debounce_count_min(
	void);
extern unsigned int mt6358_upmu_set_auxadc_lbat2_debt_max(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_lbat2_debt_min(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_lbat2_det_prd_15_0(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_lbat2_det_prd_19_16(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_lbat2_volt_max(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_lbat2_irq_en_max(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_lbat2_en_max(
	unsigned int val);
extern unsigned int mt6358_upmu_get_auxadc_lbat2_max_irq_b(
	void);
extern unsigned int mt6358_upmu_set_auxadc_lbat2_volt_min(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_lbat2_irq_en_min(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_lbat2_en_min(
	unsigned int val);
extern unsigned int mt6358_upmu_get_auxadc_lbat2_min_irq_b(
	void);
extern unsigned int mt6358_upmu_get_auxadc_lbat2_debounce_count_max(
	void);
extern unsigned int mt6358_upmu_get_auxadc_lbat2_debounce_count_min(
	void);
extern unsigned int mt6358_upmu_set_auxadc_mdrt_det_prd(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_mdrt_det_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_mdrt_det_wkup_start_cnt(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_mdrt_det_wkup_start_clr(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_mdrt_det_wkup_start(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_mdrt_det_wkup_start_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_mdrt_det_wkup_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_mdrt_det_srclken_ind(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_mdrt_det_rdy_st_prd(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_mdrt_det_rdy_st_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_mdrt_det_start_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_dcxo_mdrt_det_wkup_start_cnt(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_dcxo_mdrt_det_wkup_start_clr(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_dcxo_mdrt_det_wkup_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_dcxo_mdrt_det_wkup_start_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_dcxo_mdrt_det_wkup_start(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_nag_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_nag_clr(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_nag_vbat1_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_nag_prd(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_nag_irq_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_auxadc_nag_c_dltv_irq(
	void);
extern unsigned int mt6358_upmu_set_auxadc_nag_zcv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_nag_c_dltv_th_15_0(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_nag_c_dltv_th_26_16(
	unsigned int val);
extern unsigned int mt6358_upmu_get_auxadc_nag_cnt_15_0(
	void);
extern unsigned int mt6358_upmu_get_auxadc_nag_cnt_25_16(
	void);
extern unsigned int mt6358_upmu_get_auxadc_nag_dltv(
	void);
extern unsigned int mt6358_upmu_get_auxadc_nag_c_dltv_15_0(
	void);
extern unsigned int mt6358_upmu_get_auxadc_nag_c_dltv_26_16(
	void);
extern unsigned int mt6358_upmu_get_auxadc_nag_start(
	void);
extern unsigned int mt6358_upmu_get_auxadc_nag_ck_on(
	void);
extern unsigned int mt6358_upmu_set_auxadc_rsv_1rsv0(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_new_priority_list_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_imp_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_imp_ck_sw_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_lbat_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_lbat_ck_sw_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_bat_temp_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_bat_temp_ck_sw_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_lbat2_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_lbat2_ck_sw_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_nag_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auxadc_nag_ck_sw_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_auxadc_sample_list_15_0(
	void);
extern unsigned int mt6358_upmu_get_auxadc_sample_list_31_16(
	void);
extern unsigned int mt6358_upmu_get_auxadc_sample_list_33_32(
	void);
extern unsigned int mt6358_upmu_set_rg_buck32k_ck_pdn(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck1m_ck_pdn(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck26m_ck_pdn(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_ana_auto_off_dis(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_ana_ck_pdn(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck32k_ck_pdn_hwen(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck1m_ck_pdn_hwen(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck26m_ck_pdn_hwen(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_dcm_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_en_vproc11_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_vproc11_oc(
	void);
extern unsigned int mt6358_upmu_set_rg_int_en_vproc12_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_vproc12_oc(
	void);
extern unsigned int mt6358_upmu_set_rg_int_en_vcore_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_vcore_oc(
	void);
extern unsigned int mt6358_upmu_set_rg_int_en_vgpu_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_vgpu_oc(
	void);
extern unsigned int mt6358_upmu_set_rg_int_en_vmodem_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_vmodem_oc(
	void);
extern unsigned int mt6358_upmu_set_rg_int_en_vdram1_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_vdram1_oc(
	void);
extern unsigned int mt6358_upmu_set_rg_int_en_vs1_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_vs1_oc(
	void);
extern unsigned int mt6358_upmu_set_rg_int_en_vs2_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_vs2_oc(
	void);
extern unsigned int mt6358_upmu_set_rg_int_en_vpa_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_vpa_oc(
	void);
extern unsigned int mt6358_upmu_set_rg_int_en_vcore_preoc(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_vcore_preoc(
	void);
extern unsigned int mt6358_upmu_set_rg_int_mask_vproc11_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_vproc12_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_vcore_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_vgpu_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_vmodem_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_vdram1_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_vs1_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_vs2_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_vpa_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_vcore_preoc(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_vproc11_oc(
	void);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_vproc12_oc(
	void);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_vcore_oc(
	void);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_vgpu_oc(
	void);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_vmodem_oc(
	void);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_vdram1_oc(
	void);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_vs1_oc(
	void);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_vs2_oc(
	void);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_vpa_oc(
	void);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_vcore_preoc(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_stb_max(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_lp_prot_disable(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vsleep_src0(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vsleep_src1(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_r2r_src0(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_r2r_src1(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_lp_seq_count(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_on_seq_count(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_minfreq_latency_max(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_minfreq_duration_max(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_k_rst_done(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_k_map_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_k_once_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_k_once_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_k_once(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_k_start_manual(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_k_src_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_k_auto_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_k_auto_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_k_inv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_k_ck_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_k_ck_en(
	void);
extern unsigned int mt6358_upmu_get_buck_k_result(
	void);
extern unsigned int mt6358_upmu_get_buck_k_done(
	void);
extern unsigned int mt6358_upmu_get_buck_k_control(
	void);
extern unsigned int mt6358_upmu_get_da_smps_osc_cal(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_k_buck_ck_cnt(
	unsigned int val);
extern unsigned int mt6358_upmu_get_buck_vproc11_wdtdbg_vosel(
	void);
extern unsigned int mt6358_upmu_get_buck_vproc12_wdtdbg_vosel(
	void);
extern unsigned int mt6358_upmu_get_buck_vcore_wdtdbg_vosel(
	void);
extern unsigned int mt6358_upmu_get_buck_vgpu_wdtdbg_vosel(
	void);
extern unsigned int mt6358_upmu_get_buck_vmodem_wdtdbg_vosel(
	void);
extern unsigned int mt6358_upmu_get_buck_vdram1_wdtdbg_vosel(
	void);
extern unsigned int mt6358_upmu_get_buck_vs1_wdtdbg_vosel(
	void);
extern unsigned int mt6358_upmu_get_buck_vs2_wdtdbg_vosel(
	void);
extern unsigned int mt6358_upmu_get_buck_vpa_wdtdbg_vosel(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vproc11_oc_sdn_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vproc11_oc_sdn_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vproc12_oc_sdn_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vproc12_oc_sdn_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vcore_oc_sdn_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vcore_oc_sdn_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vgpu_oc_sdn_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vgpu_oc_sdn_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vmodem_oc_sdn_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vmodem_oc_sdn_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vdram1_oc_sdn_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vdram1_oc_sdn_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vs1_oc_sdn_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vs1_oc_sdn_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vs2_oc_sdn_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vs2_oc_sdn_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vpa_oc_sdn_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vpa_oc_sdn_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_oc_sdn_en_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_oc_sdn_en_sel(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_k_control_smps(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vproc11_vosel_limit_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vproc11_vosel_limit_sel(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vproc12_vosel_limit_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vproc12_vosel_limit_sel(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vcore_vosel_limit_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vcore_vosel_limit_sel(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vgpu_vosel_limit_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vgpu_vosel_limit_sel(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vmodem_vosel_limit_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vmodem_vosel_limit_sel(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vdram1_vosel_limit_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vdram1_vosel_limit_sel(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vs1_vosel_limit_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vs1_vosel_limit_sel(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vs2_vosel_limit_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vs2_vosel_limit_sel(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vpa_vosel_limit_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vpa_vosel_limit_sel(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vdram1_vosel_lp3(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vdram1_vosel_lp3(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vproc11_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vproc11_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vproc11_lp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vproc11_vosel_sleep(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vproc11_vosel_sleep(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vproc11_sfchg_frate(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vproc11_sfchg_fen(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vproc11_sfchg_rrate(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vproc11_sfchg_ren(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vproc11_dvs_en_td(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vproc11_dvs_en_td(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vproc11_dvs_en_ctrl(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vproc11_dvs_en_ctrl(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vproc11_dvs_en_once(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vproc11_dvs_en_once(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vproc11_dvs_down_td(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vproc11_dvs_down_ctrl(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vproc11_dvs_down_once(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vproc11_sw_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vproc11_sw_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vproc11_hw0_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vproc11_hw0_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vproc11_hw1_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vproc11_hw1_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vproc11_hw2_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vproc11_hw2_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vproc11_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vproc11_hw0_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vproc11_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vproc11_hw1_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vproc11_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vproc11_hw2_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vproc11_oc_deg_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vproc11_oc_deg_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vproc11_oc_wnd(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vproc11_oc_thd(
	unsigned int val);
extern unsigned int mt6358_upmu_get_da_vproc11_vosel(
	void);
extern unsigned int mt6358_upmu_get_da_vproc11_vosel_gray(
	void);
extern unsigned int mt6358_upmu_get_da_vproc11_en(
	void);
extern unsigned int mt6358_upmu_get_da_vproc11_stb(
	void);
extern unsigned int mt6358_upmu_get_da_vproc11_loop_sel(
	void);
extern unsigned int mt6358_upmu_get_da_vproc11_r2r_pdn(
	void);
extern unsigned int mt6358_upmu_get_da_vproc11_dvs_en(
	void);
extern unsigned int mt6358_upmu_get_da_vproc11_dvs_down(
	void);
extern unsigned int mt6358_upmu_get_da_vproc11_ssh(
	void);
extern unsigned int mt6358_upmu_get_da_vproc11_minfreq_discharge(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vproc11_oc_flag_clr_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vproc11_osc_sel_dis(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vproc11_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vproc11_ck_sw_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vproc11_ck_sw_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vproc11_vosel(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vproc11_vosel(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vproc12_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vproc12_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vproc12_lp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vproc12_vosel_sleep(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vproc12_vosel_sleep(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vproc12_sfchg_frate(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vproc12_sfchg_fen(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vproc12_sfchg_rrate(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vproc12_sfchg_ren(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vproc12_dvs_en_td(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vproc12_dvs_en_td(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vproc12_dvs_en_ctrl(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vproc12_dvs_en_ctrl(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vproc12_dvs_en_once(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vproc12_dvs_en_once(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vproc12_dvs_down_td(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vproc12_dvs_down_ctrl(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vproc12_dvs_down_once(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vproc12_sw_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vproc12_sw_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vproc12_hw0_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vproc12_hw0_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vproc12_hw1_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vproc12_hw1_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vproc12_hw2_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vproc12_hw2_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vproc12_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vproc12_hw0_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vproc12_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vproc12_hw1_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vproc12_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vproc12_hw2_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vproc12_oc_deg_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vproc12_oc_deg_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vproc12_oc_wnd(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vproc12_oc_thd(
	unsigned int val);
extern unsigned int mt6358_upmu_get_da_vproc12_vosel(
	void);
extern unsigned int mt6358_upmu_get_da_vproc12_vosel_gray(
	void);
extern unsigned int mt6358_upmu_get_da_vproc12_en(
	void);
extern unsigned int mt6358_upmu_get_da_vproc12_stb(
	void);
extern unsigned int mt6358_upmu_get_da_vproc12_loop_sel(
	void);
extern unsigned int mt6358_upmu_get_da_vproc12_r2r_pdn(
	void);
extern unsigned int mt6358_upmu_get_da_vproc12_dvs_en(
	void);
extern unsigned int mt6358_upmu_get_da_vproc12_dvs_down(
	void);
extern unsigned int mt6358_upmu_get_da_vproc12_ssh(
	void);
extern unsigned int mt6358_upmu_get_da_vproc12_minfreq_discharge(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vproc12_oc_flag_clr_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vproc12_osc_sel_dis(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vproc12_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vproc12_ck_sw_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vproc12_ck_sw_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vproc12_vosel(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vproc12_vosel(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vcore_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vcore_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vcore_lp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vcore_vosel_sleep(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vcore_vosel_sleep(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vcore_sfchg_frate(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vcore_sfchg_fen(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vcore_sfchg_rrate(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vcore_sfchg_ren(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vcore_dvs_en_td(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vcore_dvs_en_td(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vcore_dvs_en_ctrl(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vcore_dvs_en_ctrl(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vcore_dvs_en_once(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vcore_dvs_en_once(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vcore_dvs_down_td(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vcore_dvs_down_ctrl(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vcore_dvs_down_once(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vcore_sw_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vcore_sw_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vcore_hw0_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vcore_hw0_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vcore_hw1_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vcore_hw1_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vcore_hw2_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vcore_hw2_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vcore_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vcore_hw0_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vcore_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vcore_hw1_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vcore_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vcore_hw2_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vcore_oc_deg_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vcore_oc_deg_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vcore_oc_wnd(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vcore_oc_thd(
	unsigned int val);
extern unsigned int mt6358_upmu_get_da_vcore_vosel(
	void);
extern unsigned int mt6358_upmu_get_da_vcore_vosel_gray(
	void);
extern unsigned int mt6358_upmu_get_da_vcore_en(
	void);
extern unsigned int mt6358_upmu_get_da_vcore_stb(
	void);
extern unsigned int mt6358_upmu_get_da_vcore_loop_sel(
	void);
extern unsigned int mt6358_upmu_get_da_vcore_r2r_pdn(
	void);
extern unsigned int mt6358_upmu_get_da_vcore_dvs_en(
	void);
extern unsigned int mt6358_upmu_get_da_vcore_dvs_down(
	void);
extern unsigned int mt6358_upmu_get_da_vcore_ssh(
	void);
extern unsigned int mt6358_upmu_get_da_vcore_minfreq_discharge(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vcore_oc_flag_clr_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vcore_osc_sel_dis(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vcore_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vcore_ck_sw_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vcore_ck_sw_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vcore_sshub_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vcore_sshub_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vcore_sshub_sleep_vosel_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vcore_sshub_sleep_vosel_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vcore_sshub_vosel(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vcore_sshub_vosel(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vcore_sshub_vosel_sleep(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vcore_sshub_vosel_sleep(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vcore_vosel(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vcore_vosel(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vgpu_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vgpu_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vgpu_lp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vgpu_vosel_sleep(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vgpu_vosel_sleep(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vgpu_sfchg_frate(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vgpu_sfchg_fen(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vgpu_sfchg_rrate(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vgpu_sfchg_ren(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vgpu_dvs_en_td(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vgpu_dvs_en_td(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vgpu_dvs_en_ctrl(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vgpu_dvs_en_ctrl(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vgpu_dvs_en_once(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vgpu_dvs_en_once(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vgpu_dvs_down_td(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vgpu_dvs_down_ctrl(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vgpu_dvs_down_once(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vgpu_sw_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vgpu_sw_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vgpu_hw0_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vgpu_hw0_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vgpu_hw1_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vgpu_hw1_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vgpu_hw2_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vgpu_hw2_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vgpu_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vgpu_hw0_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vgpu_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vgpu_hw1_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vgpu_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vgpu_hw2_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vgpu_oc_deg_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vgpu_oc_deg_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vgpu_oc_wnd(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vgpu_oc_thd(
	unsigned int val);
extern unsigned int mt6358_upmu_get_da_vgpu_vosel(
	void);
extern unsigned int mt6358_upmu_get_da_vgpu_vosel_gray(
	void);
extern unsigned int mt6358_upmu_get_da_vgpu_en(
	void);
extern unsigned int mt6358_upmu_get_da_vgpu_stb(
	void);
extern unsigned int mt6358_upmu_get_da_vgpu_loop_sel(
	void);
extern unsigned int mt6358_upmu_get_da_vgpu_r2r_pdn(
	void);
extern unsigned int mt6358_upmu_get_da_vgpu_dvs_en(
	void);
extern unsigned int mt6358_upmu_get_da_vgpu_dvs_down(
	void);
extern unsigned int mt6358_upmu_get_da_vgpu_ssh(
	void);
extern unsigned int mt6358_upmu_get_da_vgpu_minfreq_discharge(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vgpu_oc_flag_clr_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vgpu_osc_sel_dis(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vgpu_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vgpu_ck_sw_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vgpu_ck_sw_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vgpu_vosel(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vgpu_vosel(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vmodem_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vmodem_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vmodem_lp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vmodem_vosel_sleep(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vmodem_vosel_sleep(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vmodem_sfchg_frate(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vmodem_sfchg_fen(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vmodem_sfchg_rrate(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vmodem_sfchg_ren(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vmodem_dvs_en_td(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vmodem_dvs_en_td(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vmodem_dvs_en_ctrl(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vmodem_dvs_en_ctrl(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vmodem_dvs_en_once(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vmodem_dvs_en_once(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vmodem_dvs_down_td(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vmodem_dvs_down_ctrl(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vmodem_dvs_down_once(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vmodem_sw_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vmodem_sw_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vmodem_hw0_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vmodem_hw0_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vmodem_hw1_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vmodem_hw1_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vmodem_hw2_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vmodem_hw2_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vmodem_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vmodem_hw0_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vmodem_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vmodem_hw1_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vmodem_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vmodem_hw2_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vmodem_oc_deg_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vmodem_oc_deg_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vmodem_oc_wnd(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vmodem_oc_thd(
	unsigned int val);
extern unsigned int mt6358_upmu_get_da_vmodem_vosel(
	void);
extern unsigned int mt6358_upmu_get_da_vmodem_vosel_gray(
	void);
extern unsigned int mt6358_upmu_get_da_vmodem_en(
	void);
extern unsigned int mt6358_upmu_get_da_vmodem_stb(
	void);
extern unsigned int mt6358_upmu_get_da_vmodem_loop_sel(
	void);
extern unsigned int mt6358_upmu_get_da_vmodem_r2r_pdn(
	void);
extern unsigned int mt6358_upmu_get_da_vmodem_dvs_en(
	void);
extern unsigned int mt6358_upmu_get_da_vmodem_dvs_down(
	void);
extern unsigned int mt6358_upmu_get_da_vmodem_ssh(
	void);
extern unsigned int mt6358_upmu_get_da_vmodem_minfreq_discharge(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vmodem_oc_flag_clr_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vmodem_osc_sel_dis(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vmodem_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vmodem_ck_sw_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vmodem_ck_sw_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vmodem_vosel(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vmodem_vosel(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vdram1_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vdram1_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vdram1_lp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vdram1_vosel_sleep(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vdram1_vosel_sleep(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vdram1_sfchg_frate(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vdram1_sfchg_fen(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vdram1_sfchg_rrate(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vdram1_sfchg_ren(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vdram1_dvs_en_td(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vdram1_dvs_en_td(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vdram1_dvs_en_ctrl(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vdram1_dvs_en_ctrl(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vdram1_dvs_en_once(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vdram1_dvs_en_once(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vdram1_dvs_down_td(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vdram1_dvs_down_ctrl(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vdram1_dvs_down_once(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vdram1_sw_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vdram1_sw_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vdram1_hw0_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vdram1_hw0_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vdram1_hw1_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vdram1_hw1_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vdram1_hw2_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vdram1_hw2_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vdram1_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vdram1_hw0_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vdram1_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vdram1_hw1_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vdram1_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vdram1_hw2_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vdram1_oc_deg_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vdram1_oc_deg_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vdram1_oc_wnd(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vdram1_oc_thd(
	unsigned int val);
extern unsigned int mt6358_upmu_get_da_vdram1_vosel(
	void);
extern unsigned int mt6358_upmu_get_da_vdram1_vosel_gray(
	void);
extern unsigned int mt6358_upmu_get_da_vdram1_en(
	void);
extern unsigned int mt6358_upmu_get_da_vdram1_stb(
	void);
extern unsigned int mt6358_upmu_get_da_vdram1_loop_sel(
	void);
extern unsigned int mt6358_upmu_get_da_vdram1_r2r_pdn(
	void);
extern unsigned int mt6358_upmu_get_da_vdram1_dvs_en(
	void);
extern unsigned int mt6358_upmu_get_da_vdram1_dvs_down(
	void);
extern unsigned int mt6358_upmu_get_da_vdram1_ssh(
	void);
extern unsigned int mt6358_upmu_get_da_vdram1_minfreq_discharge(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vdram1_oc_flag_clr_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vdram1_osc_sel_dis(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vdram1_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vdram1_ck_sw_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vdram1_ck_sw_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vdram1_vosel(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vdram1_vosel(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vs1_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vs1_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vs1_lp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vs1_vosel_sleep(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vs1_vosel_sleep(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vs1_sfchg_frate(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vs1_sfchg_fen(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vs1_sfchg_rrate(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vs1_sfchg_ren(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vs1_dvs_en_td(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vs1_dvs_en_td(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vs1_dvs_en_ctrl(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vs1_dvs_en_ctrl(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vs1_dvs_en_once(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vs1_dvs_en_once(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vs1_dvs_down_td(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vs1_dvs_down_ctrl(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vs1_dvs_down_once(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vs1_sw_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vs1_sw_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vs1_hw0_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vs1_hw0_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vs1_hw1_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vs1_hw1_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vs1_hw2_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vs1_hw2_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vs1_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vs1_hw0_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vs1_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vs1_hw1_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vs1_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vs1_hw2_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vs1_oc_deg_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vs1_oc_deg_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vs1_oc_wnd(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vs1_oc_thd(
	unsigned int val);
extern unsigned int mt6358_upmu_get_da_vs1_vosel(
	void);
extern unsigned int mt6358_upmu_get_da_vs1_vosel_gray(
	void);
extern unsigned int mt6358_upmu_get_da_vs1_en(
	void);
extern unsigned int mt6358_upmu_get_da_vs1_stb(
	void);
extern unsigned int mt6358_upmu_get_da_vs1_loop_sel(
	void);
extern unsigned int mt6358_upmu_get_da_vs1_r2r_pdn(
	void);
extern unsigned int mt6358_upmu_get_da_vs1_dvs_en(
	void);
extern unsigned int mt6358_upmu_get_da_vs1_dvs_down(
	void);
extern unsigned int mt6358_upmu_get_da_vs1_ssh(
	void);
extern unsigned int mt6358_upmu_get_da_vs1_minfreq_discharge(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vs1_oc_flag_clr_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vs1_osc_sel_dis(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vs1_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vs1_ck_sw_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vs1_ck_sw_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vs1_voter_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vs1_voter_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vs1_voter_vosel(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vs1_voter_vosel(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vs1_vosel(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vs1_vosel(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vs2_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vs2_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vs2_lp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vs2_vosel_sleep(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vs2_vosel_sleep(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vs2_sfchg_frate(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vs2_sfchg_fen(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vs2_sfchg_rrate(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vs2_sfchg_ren(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vs2_dvs_en_td(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vs2_dvs_en_td(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vs2_dvs_en_ctrl(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vs2_dvs_en_ctrl(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vs2_dvs_en_once(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vs2_dvs_en_once(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vs2_dvs_down_td(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vs2_dvs_down_ctrl(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vs2_dvs_down_once(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vs2_sw_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vs2_sw_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vs2_hw0_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vs2_hw0_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vs2_hw1_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vs2_hw1_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vs2_hw2_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vs2_hw2_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vs2_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vs2_hw0_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vs2_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vs2_hw1_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vs2_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vs2_hw2_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vs2_oc_deg_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vs2_oc_deg_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vs2_oc_wnd(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vs2_oc_thd(
	unsigned int val);
extern unsigned int mt6358_upmu_get_da_vs2_vosel(
	void);
extern unsigned int mt6358_upmu_get_da_vs2_vosel_gray(
	void);
extern unsigned int mt6358_upmu_get_da_vs2_en(
	void);
extern unsigned int mt6358_upmu_get_da_vs2_stb(
	void);
extern unsigned int mt6358_upmu_get_da_vs2_loop_sel(
	void);
extern unsigned int mt6358_upmu_get_da_vs2_r2r_pdn(
	void);
extern unsigned int mt6358_upmu_get_da_vs2_dvs_en(
	void);
extern unsigned int mt6358_upmu_get_da_vs2_dvs_down(
	void);
extern unsigned int mt6358_upmu_get_da_vs2_ssh(
	void);
extern unsigned int mt6358_upmu_get_da_vs2_minfreq_discharge(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vs2_oc_flag_clr_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vs2_osc_sel_dis(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vs2_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vs2_ck_sw_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vs2_ck_sw_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vs2_voter_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vs2_voter_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vs2_voter_vosel(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vs2_voter_vosel(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vs2_vosel(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vs2_vosel(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vpa_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vpa_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vpa_vosel(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vpa_vosel(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vpa_sfchg_frate(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vpa_sfchg_fen(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vpa_sfchg_rrate(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vpa_sfchg_ren(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vpa_dvs_transt_td(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vpa_dvs_transt_ctrl(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vpa_dvs_transt_once(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vpa_dvs_bw_td(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vpa_dvs_bw_ctrl(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vpa_dvs_bw_once(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vpa_oc_deg_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vpa_oc_deg_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vpa_oc_wnd(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vpa_oc_thd(
	unsigned int val);
extern unsigned int mt6358_upmu_get_da_vpa_vosel(
	void);
extern unsigned int mt6358_upmu_get_da_vpa_vosel_gray(
	void);
extern unsigned int mt6358_upmu_get_da_vpa_en(
	void);
extern unsigned int mt6358_upmu_get_da_vpa_stb(
	void);
extern unsigned int mt6358_upmu_get_da_vpa_dvs_transt(
	void);
extern unsigned int mt6358_upmu_get_da_vpa_dvs_bw(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vpa_oc_flag_clr_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vpa_osc_sel_dis(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vpa_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_vpa_ck_sw_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vpa_ck_sw_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vpa_vosel_dlc011(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vpa_vosel_dlc011(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vpa_vosel_dlc111(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vpa_vosel_dlc111(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vpa_vosel_dlc001(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vpa_vosel_dlc001(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vpa_dlc_map_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vpa_dlc_map_en(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vpa_dlc(
	unsigned int val);
extern unsigned int mt6358_upmu_get_da_vpa_dlc(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_vpa_msfg_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_buck_vpa_msfg_en(
	void);
extern unsigned int mt6358_upmu_set_rg_smps_testmode_b(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vpa_bursth(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vpa_burstl(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_smps_ivgd_det(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_autok_rst(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vmodem_sleep_voltage(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vproc11_fpwm(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vproc12_fpwm(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vproc11_ndis_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vproc11_ndis_en(
	void);
extern unsigned int mt6358_upmu_set_rg_vproc12_ndis_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vproc12_ndis_en(
	void);
extern unsigned int mt6358_upmu_set_rg_vproc11_fcot(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vproc12_fcot(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vproc11vproc12_tmdl(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vproc11_tbdis(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vproc12_tbdis(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vproc11_vdiffoff(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vproc12_vdiffoff(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vproc11_rcomp0(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vproc11_rcomp1(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vproc11_ccomp0(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vproc11_ccomp1(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vproc11_ramp_slp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vproc12_rcomp0(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vproc12_rcomp1(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vproc12_ccomp0(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vproc12_ccomp1(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vproc12_ramp_slp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vproc11_rcs(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vproc12_rcs(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vproc11_rcb(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vproc12_rcb(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vproc11_tb_width(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vproc12_tb_width(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vproc11_ug_sr(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vproc11_lg_sr(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vproc12_ug_sr(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vproc12_lg_sr(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vproc11_pfm_ton(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vproc12_pfm_ton(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rgs_vproc11_oc_status(
	void);
extern unsigned int mt6358_upmu_get_rgs_vproc12_oc_status(
	void);
extern unsigned int mt6358_upmu_get_rgs_vproc11_preoc_status(
	void);
extern unsigned int mt6358_upmu_get_rgs_vproc11_dig_mon(
	void);
extern unsigned int mt6358_upmu_get_rgs_vproc11_enpwm_status(
	void);
extern unsigned int mt6358_upmu_get_rgs_vproc12_enpwm_status(
	void);
extern unsigned int mt6358_upmu_get_rgs_vproc12_dig_mon(
	void);
extern unsigned int mt6358_upmu_set_rg_vproc11_tran_bst(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vproc12_tran_bst(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vproc11_cotramp_slp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vproc12_cotramp_slp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vproc11_sleep_time(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vproc12_sleep_time(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vproc11_vreftb(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vproc12_vreftb(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vproc11_fugon(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vproc12_fugon(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vproc11_flgon(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vproc12_flgon(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vproc11_rsv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vproc12_rsv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vproc11vproc12_disautok(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rgs_vproc11_pfm_flag(
	void);
extern unsigned int mt6358_upmu_get_rgs_vproc12_pfm_flag(
	void);
extern unsigned int mt6358_upmu_set_rg_vproc12_nonaudible_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vproc12_nonaudible_en(
	void);
extern unsigned int mt6358_upmu_set_rg_vproc11_nonaudible_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vproc11_nonaudible_en(
	void);
extern unsigned int mt6358_upmu_set_rg_vproc11_csp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vproc11_csn(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vproc12_csp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vproc12_csn(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vproc11_sonic_pfm_ton(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vproc12_sonic_pfm_ton(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vproc11_retention_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vproc11_retention_en(
	void);
extern unsigned int mt6358_upmu_set_rg_vproc12_retention_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vproc12_retention_en(
	void);
extern unsigned int mt6358_upmu_get_rgs_vproc11vproc12_trimok_status(
	void);
extern unsigned int mt6358_upmu_set_rg_vcore_fpwm(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vgpu_fpwm(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vcore_ndis_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vcore_ndis_en(
	void);
extern unsigned int mt6358_upmu_set_rg_vgpu_ndis_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vgpu_ndis_en(
	void);
extern unsigned int mt6358_upmu_set_rg_vcore_fcot(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vgpu_fcot(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vcorevgpu_tmdl(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vcore_tbdis(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vgpu_tbdis(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vcore_vdiffoff(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vgpu_vdiffoff(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vcore_rcomp0(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vcore_rcomp1(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vcore_ccomp0(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vcore_ccomp1(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vcore_ramp_slp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vgpu_rcomp0(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vgpu_rcomp1(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vgpu_ccomp0(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vgpu_ccomp1(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vgpu_ramp_slp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vcore_rcs(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vgpu_rcs(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vcore_rcb(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vgpu_rcb(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vcore_tb_width(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vgpu_tb_width(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vcore_ug_sr(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vcore_lg_sr(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vgpu_ug_sr(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vgpu_lg_sr(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vcore_pfm_ton(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vgpu_pfm_ton(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rgs_vcore_oc_status(
	void);
extern unsigned int mt6358_upmu_get_rgs_vgpu_oc_status(
	void);
extern unsigned int mt6358_upmu_get_rgs_vcore_preoc_status(
	void);
extern unsigned int mt6358_upmu_get_rgs_vcore_dig_mon(
	void);
extern unsigned int mt6358_upmu_get_rgs_vcore_enpwm_status(
	void);
extern unsigned int mt6358_upmu_get_rgs_vgpu_enpwm_status(
	void);
extern unsigned int mt6358_upmu_get_rgs_vgpu_dig_mon(
	void);
extern unsigned int mt6358_upmu_set_rg_vcore_tran_bst(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vgpu_tran_bst(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vcore_cotramp_slp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vgpu_cotramp_slp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vcore_sleep_time(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vgpu_sleep_time(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vcore_vreftb(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vgpu_vreftb(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vcore_fugon(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vgpu_fugon(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vcore_flgon(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vgpu_flgon(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vcore_rsv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vgpu_rsv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vcorevgpu_disautok(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rgs_vcore_pfm_flag(
	void);
extern unsigned int mt6358_upmu_get_rgs_vgpu_pfm_flag(
	void);
extern unsigned int mt6358_upmu_set_rg_vgpu_nonaudible_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vgpu_nonaudible_en(
	void);
extern unsigned int mt6358_upmu_set_rg_vcore_nonaudible_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vcore_nonaudible_en(
	void);
extern unsigned int mt6358_upmu_set_rg_vcore_csp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vcore_csn(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vgpu_csp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vgpu_csn(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vcore_sonic_pfm_ton(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vgpu_sonic_pfm_ton(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vcore_retention_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vcore_retention_en(
	void);
extern unsigned int mt6358_upmu_set_rg_vgpu_retention_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vgpu_retention_en(
	void);
extern unsigned int mt6358_upmu_get_rgs_vcorevgpu_trimok_status(
	void);
extern unsigned int mt6358_upmu_set_rg_vs1_trimh(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vs2_trimh(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vmodem_trimh(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vmodem_vsleep_trim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vgpu_trimh(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vcore_trimh(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vproc11_trimh(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vproc12_trimh(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vpa_trimh(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vpa_triml(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vpa_trim_ref(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vdram1_trimh(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vsram_proc11_trimh(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vsram_gpu_trimh(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vsram_others_trimh(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_voutdet_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_voutdet_en(
	void);
extern unsigned int mt6358_upmu_set_rg_vsram_proc12_trimh(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vproc11_zc_trim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vproc11_nlim_trim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vproc11_ton_trim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vproc11_rpsi1_trim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vproc11_preoc_trim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vproc11_cspslp_trim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vproc11_csnslp_trim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vproc12_zc_trim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vproc12_nlim_trim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vproc12_ton_trim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vproc12_rpsi1_trim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vproc12_cspslp_trim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vproc12_csnslp_trim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vproc11vproc12_disconfig20(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vproc11_ldo_auto_track(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vproc12_ldo_auto_track(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vcore_zc_trim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vcore_nlim_trim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vcore_ton_trim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vcore_rpsi1_trim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vcore_preoc_trim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vcore_cspslp_trim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vcore_csnslp_trim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vgpu_zc_trim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vgpu_nlim_trim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vgpu_ton_trim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vgpu_rpsi1_trim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vgpu_cspslp_trim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vgpu_csnslp_trim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vcorevgpu_disconfig20(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vcore_ldo_auto_track(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vgpu_ldo_auto_track(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vmodem_fcot(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vmodem_rcomp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vmodem_tb_dis(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vmodem_dispg(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vmodem_fpwm(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vmodem_pfm_ton(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vmodem_pwmramp_slp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vmodem_cotramp_slp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vmodem_rcs(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vmodem_sleep_time(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vmodem_nlim_gating(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vmodem_vdiff_off(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vmodem_vrefup(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vmodem_tb_width(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vmodem_ug_sr(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vmodem_lg_sr(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vmodem_ccomp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vmodem_ndis_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vmodem_ndis_en(
	void);
extern unsigned int mt6358_upmu_set_rg_vmodem_tmdl(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vmodem_rsv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vmodem_fugon(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vmodem_flgon(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vmodem_vdiffpfm_off(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rgs_vmodem_oc_status(
	void);
extern unsigned int mt6358_upmu_get_rgs_vmodem_enpwm_status(
	void);
extern unsigned int mt6358_upmu_set_rg_vmodem_disautok(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rgs_vmodem_trimok_status(
	void);
extern unsigned int mt6358_upmu_get_rgs_vmodem_dig_mon(
	void);
extern unsigned int mt6358_upmu_set_rg_vmodem_nonaudible_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vmodem_nonaudible_en(
	void);
extern unsigned int mt6358_upmu_get_rgs_vmodem_pfm_flag(
	void);
extern unsigned int mt6358_upmu_set_rg_vmodem_csp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vmodem_csn(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vmodem_sonic_pfm_ton(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vmodem_retention_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vmodem_retention_en(
	void);
extern unsigned int mt6358_upmu_set_rg_vdram1_fcot(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vdram1_rcomp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vdram1_tb_dis(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vdram1_dispg(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vdram1_fpwm(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vdram1_pfm_ton(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vdram1_pwmramp_slp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vdram1_cotramp_slp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vdram1_rcs(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vdram1_sleep_time(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vdram1_nlim_gating(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vdram1_vdiff_off(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vdram1_vrefup(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vdram1_tb_width(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vdram1_ug_sr(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vdram1_lg_sr(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vdram1_ccomp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vdram1_ndis_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vdram1_ndis_en(
	void);
extern unsigned int mt6358_upmu_set_rg_vdram1_tmdl(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vdram1_rsv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vdram1_fugon(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vdram1_flgon(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vdram1_vdiffpfm_off(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rgs_vdram1_oc_status(
	void);
extern unsigned int mt6358_upmu_get_rgs_vdram1_enpwm_status(
	void);
extern unsigned int mt6358_upmu_set_rg_vdram1_disautok(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rgs_vdram1_trimok_status(
	void);
extern unsigned int mt6358_upmu_get_rgs_vdram1_dig_mon(
	void);
extern unsigned int mt6358_upmu_set_rg_vdram1_nonaudible_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vdram1_nonaudible_en(
	void);
extern unsigned int mt6358_upmu_set_rg_vdram1_csp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vdram1_csn(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rgs_vdram1_pfm_flag(
	void);
extern unsigned int mt6358_upmu_set_rg_vdram1_sonic_pfm_ton(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vdram1_retention_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vdram1_retention_en(
	void);
extern unsigned int mt6358_upmu_set_rg_vs1_fcot(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vs1_rcomp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vs1_tb_dis(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vs1_dispg(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vs1_fpwm(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vs1_pfm_ton(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vs1_pwmramp_slp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vs1_cotramp_slp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vs1_rcs(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vs1_sleep_time(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vs1_nlim_gating(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vs1_vdiff_off(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vs1_vrefup(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vs1_tb_width(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vs1_ug_sr(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vs1_lg_sr(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vs1_ccomp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vs1_ndis_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vs1_ndis_en(
	void);
extern unsigned int mt6358_upmu_set_rg_vs1_tmdl(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vs1_rsv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vs1_fugon(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vs1_flgon(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vs1_vdiffpfm_off(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rgs_vs1_oc_status(
	void);
extern unsigned int mt6358_upmu_get_rgs_vs1_enpwm_status(
	void);
extern unsigned int mt6358_upmu_set_rg_vs1_disautok(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rgs_vs1_trimok_status(
	void);
extern unsigned int mt6358_upmu_get_rgs_vs1_dig_mon(
	void);
extern unsigned int mt6358_upmu_set_rg_vs1_nonaudible_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vs1_nonaudible_en(
	void);
extern unsigned int mt6358_upmu_set_rg_vs1_csp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vs1_csn(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rgs_vs1_pfm_flag(
	void);
extern unsigned int mt6358_upmu_set_rg_vs1_sonic_pfm_ton(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vs1_retention_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vs1_retention_en(
	void);
extern unsigned int mt6358_upmu_set_rg_vs2_fcot(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vs2_rcomp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vs2_tb_dis(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vs2_dispg(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vs2_fpwm(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vs2_pfm_ton(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vs2_pwmramp_slp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vs2_cotramp_slp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vs2_rcs(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vs2_sleep_time(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vs2_nlim_gating(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vs2_vdiff_off(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vs2_vrefup(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vs2_tb_width(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vs2_ug_sr(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vs2_lg_sr(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vs2_ccomp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vs2_ndis_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vs2_ndis_en(
	void);
extern unsigned int mt6358_upmu_set_rg_vs2_tmdl(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vs2_rsv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vs2_fugon(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vs2_flgon(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vs2_vdiffpfm_off(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rgs_vs2_oc_status(
	void);
extern unsigned int mt6358_upmu_get_rgs_vs2_enpwm_status(
	void);
extern unsigned int mt6358_upmu_set_rg_vs2_disautok(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rgs_vs2_trimok_status(
	void);
extern unsigned int mt6358_upmu_get_rgs_vs2_dig_mon(
	void);
extern unsigned int mt6358_upmu_set_rg_vs2_nonaudible_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vs2_nonaudible_en(
	void);
extern unsigned int mt6358_upmu_set_rg_vs2_csp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vs2_csn(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rgs_vs2_pfm_flag(
	void);
extern unsigned int mt6358_upmu_set_rg_vs2_sonic_pfm_ton(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vs2_retention_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vs2_retention_en(
	void);
extern unsigned int mt6358_upmu_set_rg_vpa_ndis_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vpa_ndis_en(
	void);
extern unsigned int mt6358_upmu_set_rg_vpa_modeset(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vpa_cc(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vpa_csr(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vpa_csmir(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vpa_csl(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vpa_slp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vpa_azc_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vpa_azc_en(
	void);
extern unsigned int mt6358_upmu_set_rg_vpa_cp_fwupoff(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vpa_azc_delay(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vpa_rzsel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vpa_hzp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vpa_bwex_gat(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vpa_slew(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vpa_slew_nmos(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vpa_min_on(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vpa_vbat_del(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rgs_vpa_azc_vos_sel(
	void);
extern unsigned int mt6358_upmu_set_rg_vpa_min_pk(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vpa_rsv1(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vpa_rsv2(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rgs_vpa_oc_status(
	void);
extern unsigned int mt6358_upmu_get_rgs_vpa_azc_zx(
	void);
extern unsigned int mt6358_upmu_get_rgs_vpa_dig_mon(
	void);
extern unsigned int mt6358_upmu_set_rg_vmodem_zc_trim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vmodem_nlim_trim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vmodem_ton_trim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vmodem_rpsi_trim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vmodem_csnslp_trim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vmodem_cspslp_trim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vdram1_zc_trim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vdram1_nlim_trim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vdram1_ton_trim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vdram1_rpsi_trim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vdram1_csnslp_trim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vdram1_cspslp_trim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vs1_zc_trim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vs1_nlim_trim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vs1_ton_trim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vs1_rpsi_trim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vs1_csnslp_trim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vs1_cspslp_trim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vs2_zc_trim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vs2_nlim_trim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vs2_ton_trim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vs2_rpsi_trim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vs2_csnslp_trim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vs2_cspslp_trim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vpa_zxref(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vpa_nlim_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_32k_ck_pdn(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_intrp_ck_pdn(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_32k_ck_pdn_hwen(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_intrp_ck_pdn_hwen(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_dcm_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vfe28_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vxo22_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vrf18_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vrf12_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vefuse_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vcn33_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vcn28_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vcn18_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vcama1_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vcama2_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vcamd_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vcamio_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vldo28_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_va12_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vaux18_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vaud28_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vio28_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vio18_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_proc11_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_proc11_osc_sel_dis(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_proc12_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_proc12_osc_sel_dis(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_others_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_others_osc_sel_dis(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_gpu_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_gpu_osc_sel_dis(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vdram2_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vmc_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vmch_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vemc_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vsim1_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vsim2_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vibr_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vusb_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vbif28_ck_sw_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_en_vfe28_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_vfe28_oc(
	void);
extern unsigned int mt6358_upmu_set_rg_int_en_vxo22_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_vxo22_oc(
	void);
extern unsigned int mt6358_upmu_set_rg_int_en_vrf18_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_vrf18_oc(
	void);
extern unsigned int mt6358_upmu_set_rg_int_en_vrf12_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_vrf12_oc(
	void);
extern unsigned int mt6358_upmu_set_rg_int_en_vefuse_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_vefuse_oc(
	void);
extern unsigned int mt6358_upmu_set_rg_int_en_vcn33_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_vcn33_oc(
	void);
extern unsigned int mt6358_upmu_set_rg_int_en_vcn28_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_vcn28_oc(
	void);
extern unsigned int mt6358_upmu_set_rg_int_en_vcn18_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_vcn18_oc(
	void);
extern unsigned int mt6358_upmu_set_rg_int_en_vcama1_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_vcama1_oc(
	void);
extern unsigned int mt6358_upmu_set_rg_int_en_vcama2_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_vcama2_oc(
	void);
extern unsigned int mt6358_upmu_set_rg_int_en_vcamd_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_vcamd_oc(
	void);
extern unsigned int mt6358_upmu_set_rg_int_en_vcamio_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_vcamio_oc(
	void);
extern unsigned int mt6358_upmu_set_rg_int_en_vldo28_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_vldo28_oc(
	void);
extern unsigned int mt6358_upmu_set_rg_int_en_va12_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_va12_oc(
	void);
extern unsigned int mt6358_upmu_set_rg_int_en_vaux18_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_vaux18_oc(
	void);
extern unsigned int mt6358_upmu_set_rg_int_en_vaud28_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_vaud28_oc(
	void);
extern unsigned int mt6358_upmu_set_rg_int_en_vio28_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_vio28_oc(
	void);
extern unsigned int mt6358_upmu_set_rg_int_en_vio18_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_vio18_oc(
	void);
extern unsigned int mt6358_upmu_set_rg_int_en_vsram_proc11_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_vsram_proc11_oc(
	void);
extern unsigned int mt6358_upmu_set_rg_int_en_vsram_proc12_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_vsram_proc12_oc(
	void);
extern unsigned int mt6358_upmu_set_rg_int_en_vsram_others_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_vsram_others_oc(
	void);
extern unsigned int mt6358_upmu_set_rg_int_en_vsram_gpu_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_vsram_gpu_oc(
	void);
extern unsigned int mt6358_upmu_set_rg_int_en_vdram2_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_vdram2_oc(
	void);
extern unsigned int mt6358_upmu_set_rg_int_en_vmc_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_vmc_oc(
	void);
extern unsigned int mt6358_upmu_set_rg_int_en_vmch_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_vmch_oc(
	void);
extern unsigned int mt6358_upmu_set_rg_int_en_vemc_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_vemc_oc(
	void);
extern unsigned int mt6358_upmu_set_rg_int_en_vsim1_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_vsim1_oc(
	void);
extern unsigned int mt6358_upmu_set_rg_int_en_vsim2_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_vsim2_oc(
	void);
extern unsigned int mt6358_upmu_set_rg_int_en_vibr_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_vibr_oc(
	void);
extern unsigned int mt6358_upmu_set_rg_int_en_vusb_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_vusb_oc(
	void);
extern unsigned int mt6358_upmu_set_rg_int_en_vbif28_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_vbif28_oc(
	void);
extern unsigned int mt6358_upmu_set_rg_int_mask_vfe28_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_vxo22_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_vrf18_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_vrf12_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_vefuse_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_vcn33_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_vcn28_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_vcn18_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_vcama1_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_vcama2_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_vcamd_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_vcamio_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_vldo28_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_va12_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_vaux18_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_vaud28_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_vio28_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_vio18_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_vsram_proc11_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_vsram_proc12_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_vsram_others_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_vsram_gpu_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_vdram2_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_vmc_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_vmch_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_vemc_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_vsim1_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_vsim2_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_vibr_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_vusb_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_vbif28_oc(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_vfe28_oc(
	void);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_vxo22_oc(
	void);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_vrf18_oc(
	void);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_vrf12_oc(
	void);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_vefuse_oc(
	void);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_vcn33_oc(
	void);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_vcn28_oc(
	void);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_vcn18_oc(
	void);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_vcama1_oc(
	void);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_vcama2_oc(
	void);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_vcamd_oc(
	void);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_vcamio_oc(
	void);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_vldo28_oc(
	void);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_va12_oc(
	void);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_vaux18_oc(
	void);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_vaud28_oc(
	void);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_vio28_oc(
	void);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_vio18_oc(
	void);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_vsram_proc11_oc(
	void);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_vsram_proc12_oc(
	void);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_vsram_others_oc(
	void);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_vsram_gpu_oc(
	void);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_vdram2_oc(
	void);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_vmc_oc(
	void);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_vmch_oc(
	void);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_vemc_oc(
	void);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_vsim1_oc(
	void);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_vsim2_oc(
	void);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_vibr_oc(
	void);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_vusb_oc(
	void);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_vbif28_oc(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_mon_flag_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_int_flag_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_int_flag_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_mon_grp_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_wdt_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_top_rsv0(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_top_rsv1(
	unsigned int val);
extern unsigned int mt6358_upmu_set_ldo_degtd_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_lp_prot_disable(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_dummy_load_gated_dis(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vxo22_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vxo22_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vxo22_lp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vxo22_sw_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vxo22_sw_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vxo22_hw0_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vxo22_hw0_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vxo22_hw1_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vxo22_hw1_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vxo22_hw2_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vxo22_hw2_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vxo22_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vxo22_hw0_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vxo22_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vxo22_hw1_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vxo22_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vxo22_hw2_op_cfg(
	void);
extern unsigned int mt6358_upmu_get_da_vxo22_mode(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vxo22_stbtd(
	unsigned int val);
extern unsigned int mt6358_upmu_get_da_vxo22_stb(
	void);
extern unsigned int mt6358_upmu_get_da_vxo22_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vxo22_ocfb_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vxo22_ocfb_en(
	void);
extern unsigned int mt6358_upmu_get_da_vxo22_ocfb_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vxo22_dummy_load(
	unsigned int val);
extern unsigned int mt6358_upmu_get_da_vxo22_dummy_load(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_va12_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_va12_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_va12_lp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_va12_sw_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_va12_sw_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_va12_hw0_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_va12_hw0_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_va12_hw1_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_va12_hw1_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_va12_hw2_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_va12_hw2_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_va12_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_va12_hw0_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_va12_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_va12_hw1_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_va12_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_va12_hw2_op_cfg(
	void);
extern unsigned int mt6358_upmu_get_da_va12_mode(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_va12_stbtd(
	unsigned int val);
extern unsigned int mt6358_upmu_get_da_va12_stb(
	void);
extern unsigned int mt6358_upmu_get_da_va12_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_va12_auxadc_pwdb_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_va12_auxadc_pwdb_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_va12_ocfb_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_va12_ocfb_en(
	void);
extern unsigned int mt6358_upmu_get_da_va12_ocfb_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_va12_dummy_load(
	unsigned int val);
extern unsigned int mt6358_upmu_get_da_va12_dummy_load(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vaux18_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vaux18_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vaux18_lp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vaux18_sw_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vaux18_sw_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vaux18_hw0_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vaux18_hw0_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vaux18_hw1_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vaux18_hw1_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vaux18_hw2_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vaux18_hw2_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vaux18_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vaux18_hw0_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vaux18_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vaux18_hw1_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vaux18_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vaux18_hw2_op_cfg(
	void);
extern unsigned int mt6358_upmu_get_da_vaux18_mode(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vaux18_stbtd(
	unsigned int val);
extern unsigned int mt6358_upmu_get_da_vaux18_stb(
	void);
extern unsigned int mt6358_upmu_get_da_vaux18_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vaux18_auxadc_pwdb_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vaux18_auxadc_pwdb_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vaux18_ocfb_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vaux18_ocfb_en(
	void);
extern unsigned int mt6358_upmu_get_da_vaux18_ocfb_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vaux18_dummy_load(
	unsigned int val);
extern unsigned int mt6358_upmu_get_da_vaux18_dummy_load(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vaud28_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vaud28_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vaud28_lp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vaud28_sw_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vaud28_sw_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vaud28_hw0_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vaud28_hw0_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vaud28_hw1_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vaud28_hw1_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vaud28_hw2_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vaud28_hw2_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vaud28_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vaud28_hw0_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vaud28_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vaud28_hw1_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vaud28_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vaud28_hw2_op_cfg(
	void);
extern unsigned int mt6358_upmu_get_da_vaud28_mode(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vaud28_stbtd(
	unsigned int val);
extern unsigned int mt6358_upmu_get_da_vaud28_stb(
	void);
extern unsigned int mt6358_upmu_get_da_vaud28_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vaud28_auxadc_pwdb_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vaud28_auxadc_pwdb_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vaud28_ocfb_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vaud28_ocfb_en(
	void);
extern unsigned int mt6358_upmu_get_da_vaud28_ocfb_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vaud28_dummy_load(
	unsigned int val);
extern unsigned int mt6358_upmu_get_da_vaud28_dummy_load(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vio28_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vio28_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vio28_lp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vio28_sw_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vio28_sw_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vio28_hw0_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vio28_hw0_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vio28_hw1_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vio28_hw1_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vio28_hw2_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vio28_hw2_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vio28_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vio28_hw0_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vio28_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vio28_hw1_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vio28_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vio28_hw2_op_cfg(
	void);
extern unsigned int mt6358_upmu_get_da_vio28_mode(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vio28_stbtd(
	unsigned int val);
extern unsigned int mt6358_upmu_get_da_vio28_stb(
	void);
extern unsigned int mt6358_upmu_get_da_vio28_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vio28_ocfb_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vio28_ocfb_en(
	void);
extern unsigned int mt6358_upmu_get_da_vio28_ocfb_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vio28_dummy_load(
	unsigned int val);
extern unsigned int mt6358_upmu_get_da_vio28_dummy_load(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vio18_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vio18_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vio18_lp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vio18_sw_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vio18_sw_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vio18_hw0_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vio18_hw0_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vio18_hw1_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vio18_hw1_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vio18_hw2_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vio18_hw2_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vio18_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vio18_hw0_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vio18_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vio18_hw1_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vio18_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vio18_hw2_op_cfg(
	void);
extern unsigned int mt6358_upmu_get_da_vio18_mode(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vio18_stbtd(
	unsigned int val);
extern unsigned int mt6358_upmu_get_da_vio18_stb(
	void);
extern unsigned int mt6358_upmu_get_da_vio18_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vio18_ocfb_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vio18_ocfb_en(
	void);
extern unsigned int mt6358_upmu_get_da_vio18_ocfb_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vio18_dummy_load(
	unsigned int val);
extern unsigned int mt6358_upmu_get_da_vio18_dummy_load(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vdram2_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vdram2_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vdram2_lp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vdram2_sw_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vdram2_sw_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vdram2_hw0_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vdram2_hw0_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vdram2_hw1_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vdram2_hw1_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vdram2_hw2_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vdram2_hw2_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vdram2_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vdram2_hw0_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vdram2_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vdram2_hw1_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vdram2_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vdram2_hw2_op_cfg(
	void);
extern unsigned int mt6358_upmu_get_da_vdram2_mode(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vdram2_stbtd(
	unsigned int val);
extern unsigned int mt6358_upmu_get_da_vdram2_stb(
	void);
extern unsigned int mt6358_upmu_get_da_vdram2_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vdram2_ocfb_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vdram2_ocfb_en(
	void);
extern unsigned int mt6358_upmu_get_da_vdram2_ocfb_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vdram2_dummy_load(
	unsigned int val);
extern unsigned int mt6358_upmu_get_da_vdram2_dummy_load(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vemc_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vemc_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vemc_lp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vemc_sw_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vemc_sw_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vemc_hw0_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vemc_hw0_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vemc_hw1_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vemc_hw1_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vemc_hw2_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vemc_hw2_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vemc_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vemc_hw0_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vemc_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vemc_hw1_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vemc_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vemc_hw2_op_cfg(
	void);
extern unsigned int mt6358_upmu_get_da_vemc_mode(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vemc_stbtd(
	unsigned int val);
extern unsigned int mt6358_upmu_get_da_vemc_stb(
	void);
extern unsigned int mt6358_upmu_get_da_vemc_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vemc_ocfb_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vemc_ocfb_en(
	void);
extern unsigned int mt6358_upmu_get_da_vemc_ocfb_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vemc_dummy_load(
	unsigned int val);
extern unsigned int mt6358_upmu_get_da_vemc_dummy_load(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vusb_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vusb_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vusb_lp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vusb_sw_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vusb_sw_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vusb_hw0_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vusb_hw0_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vusb_hw1_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vusb_hw1_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vusb_hw2_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vusb_hw2_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vusb_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vusb_hw0_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vusb_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vusb_hw1_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vusb_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vusb_hw2_op_cfg(
	void);
extern unsigned int mt6358_upmu_get_da_vusb_mode(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vusb_stbtd(
	unsigned int val);
extern unsigned int mt6358_upmu_get_da_vusb_stb(
	void);
extern unsigned int mt6358_upmu_get_da_vusb_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vusb_ocfb_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vusb_ocfb_en(
	void);
extern unsigned int mt6358_upmu_get_da_vusb_ocfb_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vusb_dummy_load(
	unsigned int val);
extern unsigned int mt6358_upmu_get_da_vusb_dummy_load(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_proc11_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsram_proc11_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_proc11_lp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_proc11_vosel_sleep(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsram_proc11_vosel_sleep(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_proc11_sfchg_frate(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_proc11_sfchg_fen(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_proc11_sfchg_rrate(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_proc11_sfchg_ren(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_proc11_dvs_trans_td(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_proc11_dvs_trans_ctrl(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_proc11_dvs_trans_once(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_proc11_sw_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsram_proc11_sw_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_proc11_hw0_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsram_proc11_hw0_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_proc11_hw1_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsram_proc11_hw1_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_proc11_hw2_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsram_proc11_hw2_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_proc11_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsram_proc11_hw0_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_proc11_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsram_proc11_hw1_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_proc11_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsram_proc11_hw2_op_cfg(
	void);
extern unsigned int mt6358_upmu_get_da_vsram_proc11_mode(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_proc11_stbtd(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_proc11_ocfb_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsram_proc11_ocfb_en(
	void);
extern unsigned int mt6358_upmu_get_da_vsram_proc11_ocfb_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_proc11_dummy_load(
	unsigned int val);
extern unsigned int mt6358_upmu_get_da_vsram_proc11_dummy_load(
	void);
extern unsigned int mt6358_upmu_get_da_vsram_proc11_vosel_gray(
	void);
extern unsigned int mt6358_upmu_get_da_vsram_proc11_vosel(
	void);
extern unsigned int mt6358_upmu_get_da_vsram_proc11_en(
	void);
extern unsigned int mt6358_upmu_get_da_vsram_proc11_stb(
	void);
extern unsigned int mt6358_upmu_get_da_vsram_proc11_vsleep_sel(
	void);
extern unsigned int mt6358_upmu_get_da_vsram_proc11_r2r_pdn(
	void);
extern unsigned int mt6358_upmu_get_da_vsram_proc11_track_ndis_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_proc11_track_sleep_ctrl(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_proc11_track_on_ctrl(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_proc11_track_vproc11_on_ctrl(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_proc11_vosel_delta(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsram_proc11_vosel_delta(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_proc11_vosel_offset(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsram_proc11_vosel_offset(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_proc11_vosel_on_lb(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsram_proc11_vosel_on_lb(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_proc11_vosel_on_hb(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsram_proc11_vosel_on_hb(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_proc11_vosel_sleep_lb(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsram_proc11_vosel_sleep_lb(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_proc12_track_sleep_ctrl(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_proc12_track_on_ctrl(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_proc12_track_vproc12_on_ctrl(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_proc12_vosel_delta(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsram_proc12_vosel_delta(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_proc12_vosel_offset(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsram_proc12_vosel_offset(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_proc12_vosel_on_lb(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsram_proc12_vosel_on_lb(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_proc12_vosel_on_hb(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsram_proc12_vosel_on_hb(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_proc12_vosel_sleep_lb(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsram_proc12_vosel_sleep_lb(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_wakeup_time(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vdram2_vosel(unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vdram2_vosel(void);
extern unsigned int mt6358_upmu_set_rg_ldo_vdram2_vosel_1(unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vdram2_vosel_1(void);
extern unsigned int mt6358_upmu_set_rg_ldo_from_strup_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_from_strup_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_proc12_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsram_proc12_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_proc12_lp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_proc12_vosel_sleep(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsram_proc12_vosel_sleep(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_proc12_sfchg_frate(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_proc12_sfchg_fen(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_proc12_sfchg_rrate(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_proc12_sfchg_ren(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_proc12_dvs_trans_td(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_proc12_dvs_trans_ctrl(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_proc12_dvs_trans_once(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_proc12_sw_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsram_proc12_sw_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_proc12_hw0_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsram_proc12_hw0_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_proc12_hw1_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsram_proc12_hw1_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_proc12_hw2_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsram_proc12_hw2_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_proc12_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsram_proc12_hw0_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_proc12_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsram_proc12_hw1_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_proc12_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsram_proc12_hw2_op_cfg(
	void);
extern unsigned int mt6358_upmu_get_da_vsram_proc12_mode(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_proc12_stbtd(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_proc12_ocfb_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsram_proc12_ocfb_en(
	void);
extern unsigned int mt6358_upmu_get_da_vsram_proc12_ocfb_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_proc12_dummy_load(
	unsigned int val);
extern unsigned int mt6358_upmu_get_da_vsram_proc12_dummy_load(
	void);
extern unsigned int mt6358_upmu_get_da_vsram_proc12_vosel_gray(
	void);
extern unsigned int mt6358_upmu_get_da_vsram_proc12_vosel(
	void);
extern unsigned int mt6358_upmu_get_da_vsram_proc12_en(
	void);
extern unsigned int mt6358_upmu_get_da_vsram_proc12_stb(
	void);
extern unsigned int mt6358_upmu_get_da_vsram_proc12_vsleep_sel(
	void);
extern unsigned int mt6358_upmu_get_da_vsram_proc12_r2r_pdn(
	void);
extern unsigned int mt6358_upmu_get_da_vsram_proc12_track_ndis_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_others_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsram_others_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_others_lp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_others_vosel_sleep(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsram_others_vosel_sleep(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_others_sfchg_frate(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_others_sfchg_fen(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_others_sfchg_rrate(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_others_sfchg_ren(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_others_dvs_trans_td(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_others_dvs_trans_ctrl(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_others_dvs_trans_once(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_others_sw_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsram_others_sw_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_others_hw0_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsram_others_hw0_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_others_hw1_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsram_others_hw1_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_others_hw2_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsram_others_hw2_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_others_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsram_others_hw0_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_others_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsram_others_hw1_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_others_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsram_others_hw2_op_cfg(
	void);
extern unsigned int mt6358_upmu_get_da_vsram_others_mode(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_others_stbtd(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_others_ocfb_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsram_others_ocfb_en(
	void);
extern unsigned int mt6358_upmu_get_da_vsram_others_ocfb_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_others_dummy_load(
	unsigned int val);
extern unsigned int mt6358_upmu_get_da_vsram_others_dummy_load(
	void);
extern unsigned int mt6358_upmu_get_da_vsram_others_vosel_gray(
	void);
extern unsigned int mt6358_upmu_get_da_vsram_others_vosel(
	void);
extern unsigned int mt6358_upmu_get_da_vsram_others_en(
	void);
extern unsigned int mt6358_upmu_get_da_vsram_others_stb(
	void);
extern unsigned int mt6358_upmu_get_da_vsram_others_vsleep_sel(
	void);
extern unsigned int mt6358_upmu_get_da_vsram_others_r2r_pdn(
	void);
extern unsigned int mt6358_upmu_get_da_vsram_others_track_ndis_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_others_sshub_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsram_others_sshub_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_others_sshub_sleep_vosel_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsram_others_sshub_sleep_vosel_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_others_sshub_vosel(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsram_others_sshub_vosel(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_others_sshub_vosel_sleep(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsram_others_sshub_vosel_sleep(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_gpu_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsram_gpu_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_gpu_lp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_gpu_vosel_sleep(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsram_gpu_vosel_sleep(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_gpu_sfchg_frate(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_gpu_sfchg_fen(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_gpu_sfchg_rrate(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_gpu_sfchg_ren(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_gpu_dvs_trans_td(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_gpu_dvs_trans_ctrl(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_gpu_dvs_trans_once(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_gpu_sw_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsram_gpu_sw_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_gpu_hw0_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsram_gpu_hw0_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_gpu_hw1_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsram_gpu_hw1_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_gpu_hw2_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsram_gpu_hw2_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_gpu_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsram_gpu_hw0_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_gpu_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsram_gpu_hw1_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_gpu_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsram_gpu_hw2_op_cfg(
	void);
extern unsigned int mt6358_upmu_get_da_vsram_gpu_mode(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_gpu_stbtd(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_gpu_ocfb_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsram_gpu_ocfb_en(
	void);
extern unsigned int mt6358_upmu_get_da_vsram_gpu_ocfb_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_gpu_dummy_load(
	unsigned int val);
extern unsigned int mt6358_upmu_get_da_vsram_gpu_dummy_load(
	void);
extern unsigned int mt6358_upmu_get_da_vsram_gpu_vosel_gray(
	void);
extern unsigned int mt6358_upmu_get_da_vsram_gpu_vosel(
	void);
extern unsigned int mt6358_upmu_get_da_vsram_gpu_en(
	void);
extern unsigned int mt6358_upmu_get_da_vsram_gpu_stb(
	void);
extern unsigned int mt6358_upmu_get_da_vsram_gpu_vsleep_sel(
	void);
extern unsigned int mt6358_upmu_get_da_vsram_gpu_r2r_pdn(
	void);
extern unsigned int mt6358_upmu_get_da_vsram_gpu_track_ndis_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_proc11_r2r_pdn_dis(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_proc12_r2r_pdn_dis(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_others_r2r_pdn_dis(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_gpu_r2r_pdn_dis(
	unsigned int val);
extern unsigned int mt6358_upmu_get_ldo_vsram_proc11_wdtdbg_vosel(
	void);
extern unsigned int mt6358_upmu_get_ldo_vsram_proc12_wdtdbg_vosel(
	void);
extern unsigned int mt6358_upmu_get_ldo_vsram_others_wdtdbg_vosel(
	void);
extern unsigned int mt6358_upmu_get_ldo_vsram_gpu_wdtdbg_vosel(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_proc11_vosel(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsram_proc11_vosel(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_proc12_vosel(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsram_proc12_vosel(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_others_vosel(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsram_others_vosel(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_gpu_vosel(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsram_gpu_vosel(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_proc11_vosel_limit_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsram_proc11_vosel_limit_sel(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_proc12_vosel_limit_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsram_proc12_vosel_limit_sel(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_others_vosel_limit_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsram_others_vosel_limit_sel(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsram_gpu_vosel_limit_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsram_gpu_vosel_limit_sel(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vfe28_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vfe28_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vfe28_lp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vfe28_sw_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vfe28_sw_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vfe28_hw0_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vfe28_hw0_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vfe28_hw1_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vfe28_hw1_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vfe28_hw2_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vfe28_hw2_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vfe28_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vfe28_hw0_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vfe28_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vfe28_hw1_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vfe28_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vfe28_hw2_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vfe28_on_op(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vfe28_lp_op(
	unsigned int val);
extern unsigned int mt6358_upmu_get_da_vfe28_mode(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vfe28_stbtd(
	unsigned int val);
extern unsigned int mt6358_upmu_get_da_vfe28_stb(
	void);
extern unsigned int mt6358_upmu_get_da_vfe28_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vfe28_ocfb_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vfe28_ocfb_en(
	void);
extern unsigned int mt6358_upmu_get_da_vfe28_ocfb_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vfe28_dummy_load(
	unsigned int val);
extern unsigned int mt6358_upmu_get_da_vfe28_dummy_load(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vrf18_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vrf18_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vrf18_lp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vrf18_sw_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vrf18_sw_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vrf18_hw0_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vrf18_hw0_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vrf18_hw1_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vrf18_hw1_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vrf18_hw2_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vrf18_hw2_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vrf18_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vrf18_hw0_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vrf18_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vrf18_hw1_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vrf18_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vrf18_hw2_op_cfg(
	void);
extern unsigned int mt6358_upmu_get_da_vrf18_mode(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vrf18_stbtd(
	unsigned int val);
extern unsigned int mt6358_upmu_get_da_vrf18_stb(
	void);
extern unsigned int mt6358_upmu_get_da_vrf18_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vrf18_ocfb_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vrf18_ocfb_en(
	void);
extern unsigned int mt6358_upmu_get_da_vrf18_ocfb_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vrf18_dummy_load(
	unsigned int val);
extern unsigned int mt6358_upmu_get_da_vrf18_dummy_load(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vrf12_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vrf12_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vrf12_lp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vrf12_sw_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vrf12_sw_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vrf12_hw0_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vrf12_hw0_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vrf12_hw1_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vrf12_hw1_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vrf12_hw2_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vrf12_hw2_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vrf12_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vrf12_hw0_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vrf12_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vrf12_hw1_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vrf12_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vrf12_hw2_op_cfg(
	void);
extern unsigned int mt6358_upmu_get_da_vrf12_mode(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vrf12_stbtd(
	unsigned int val);
extern unsigned int mt6358_upmu_get_da_vrf12_stb(
	void);
extern unsigned int mt6358_upmu_get_da_vrf12_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vrf12_ocfb_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vrf12_ocfb_en(
	void);
extern unsigned int mt6358_upmu_get_da_vrf12_ocfb_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vrf12_dummy_load(
	unsigned int val);
extern unsigned int mt6358_upmu_get_da_vrf12_dummy_load(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vefuse_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vefuse_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vefuse_lp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vefuse_sw_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vefuse_sw_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vefuse_hw0_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vefuse_hw0_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vefuse_hw1_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vefuse_hw1_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vefuse_hw2_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vefuse_hw2_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vefuse_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vefuse_hw0_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vefuse_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vefuse_hw1_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vefuse_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vefuse_hw2_op_cfg(
	void);
extern unsigned int mt6358_upmu_get_da_vefuse_mode(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vefuse_stbtd(
	unsigned int val);
extern unsigned int mt6358_upmu_get_da_vefuse_stb(
	void);
extern unsigned int mt6358_upmu_get_da_vefuse_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vefuse_ocfb_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vefuse_ocfb_en(
	void);
extern unsigned int mt6358_upmu_get_da_vefuse_ocfb_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vefuse_dummy_load(
	unsigned int val);
extern unsigned int mt6358_upmu_get_da_vefuse_dummy_load(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcn18_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcn18_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcn18_lp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vcn18_sw_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcn18_sw_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcn18_hw0_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcn18_hw0_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcn18_hw1_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcn18_hw1_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcn18_hw2_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcn18_hw2_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcn18_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcn18_hw0_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcn18_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcn18_hw1_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcn18_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcn18_hw2_op_cfg(
	void);
extern unsigned int mt6358_upmu_get_da_vcn18_mode(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcn18_stbtd(
	unsigned int val);
extern unsigned int mt6358_upmu_get_da_vcn18_stb(
	void);
extern unsigned int mt6358_upmu_get_da_vcn18_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcn18_ocfb_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcn18_ocfb_en(
	void);
extern unsigned int mt6358_upmu_get_da_vcn18_ocfb_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcn18_dummy_load(
	unsigned int val);
extern unsigned int mt6358_upmu_get_da_vcn18_dummy_load(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcama1_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcama1_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcama1_lp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vcama1_sw_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcama1_sw_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcama1_hw0_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcama1_hw0_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcama1_hw1_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcama1_hw1_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcama1_hw2_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcama1_hw2_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcama1_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcama1_hw0_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcama1_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcama1_hw1_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcama1_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcama1_hw2_op_cfg(
	void);
extern unsigned int mt6358_upmu_get_da_vcama1_mode(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcama1_stbtd(
	unsigned int val);
extern unsigned int mt6358_upmu_get_da_vcama1_stb(
	void);
extern unsigned int mt6358_upmu_get_da_vcama1_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcama1_ocfb_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcama1_ocfb_en(
	void);
extern unsigned int mt6358_upmu_get_da_vcama1_ocfb_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcama1_dummy_load(
	unsigned int val);
extern unsigned int mt6358_upmu_get_da_vcama1_dummy_load(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcama2_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcama2_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcama2_lp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vcama2_sw_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcama2_sw_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcama2_hw0_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcama2_hw0_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcama2_hw1_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcama2_hw1_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcama2_hw2_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcama2_hw2_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcama2_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcama2_hw0_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcama2_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcama2_hw1_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcama2_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcama2_hw2_op_cfg(
	void);
extern unsigned int mt6358_upmu_get_da_vcama2_mode(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcama2_stbtd(
	unsigned int val);
extern unsigned int mt6358_upmu_get_da_vcama2_stb(
	void);
extern unsigned int mt6358_upmu_get_da_vcama2_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcama2_ocfb_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcama2_ocfb_en(
	void);
extern unsigned int mt6358_upmu_get_da_vcama2_ocfb_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcama2_dummy_load(
	unsigned int val);
extern unsigned int mt6358_upmu_get_da_vcama2_dummy_load(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcamd_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcamd_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcamd_lp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vcamd_sw_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcamd_sw_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcamd_hw0_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcamd_hw0_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcamd_hw1_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcamd_hw1_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcamd_hw2_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcamd_hw2_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcamd_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcamd_hw0_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcamd_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcamd_hw1_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcamd_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcamd_hw2_op_cfg(
	void);
extern unsigned int mt6358_upmu_get_da_vcamd_mode(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcamd_stbtd(
	unsigned int val);
extern unsigned int mt6358_upmu_get_da_vcamd_stb(
	void);
extern unsigned int mt6358_upmu_get_da_vcamd_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcamd_ocfb_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcamd_ocfb_en(
	void);
extern unsigned int mt6358_upmu_get_da_vcamd_ocfb_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcamd_dummy_load(
	unsigned int val);
extern unsigned int mt6358_upmu_get_da_vcamd_dummy_load(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcamio_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcamio_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcamio_lp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vcamio_sw_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcamio_sw_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcamio_hw0_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcamio_hw0_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcamio_hw1_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcamio_hw1_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcamio_hw2_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcamio_hw2_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcamio_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcamio_hw0_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcamio_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcamio_hw1_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcamio_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcamio_hw2_op_cfg(
	void);
extern unsigned int mt6358_upmu_get_da_vcamio_mode(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcamio_stbtd(
	unsigned int val);
extern unsigned int mt6358_upmu_get_da_vcamio_stb(
	void);
extern unsigned int mt6358_upmu_get_da_vcamio_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcamio_ocfb_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcamio_ocfb_en(
	void);
extern unsigned int mt6358_upmu_get_da_vcamio_ocfb_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcamio_dummy_load(
	unsigned int val);
extern unsigned int mt6358_upmu_get_da_vcamio_dummy_load(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vmc_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vmc_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vmc_lp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vmc_sw_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vmc_sw_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vmc_hw0_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vmc_hw0_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vmc_hw1_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vmc_hw1_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vmc_hw2_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vmc_hw2_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vmc_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vmc_hw0_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vmc_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vmc_hw1_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vmc_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vmc_hw2_op_cfg(
	void);
extern unsigned int mt6358_upmu_get_da_vmc_mode(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vmc_stbtd(
	unsigned int val);
extern unsigned int mt6358_upmu_get_da_vmc_stb(
	void);
extern unsigned int mt6358_upmu_get_da_vmc_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vmc_ocfb_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vmc_ocfb_en(
	void);
extern unsigned int mt6358_upmu_get_da_vmc_ocfb_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vmc_dummy_load(
	unsigned int val);
extern unsigned int mt6358_upmu_get_da_vmc_dummy_load(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vmch_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vmch_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vmch_lp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vmch_sw_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vmch_sw_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vmch_hw0_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vmch_hw0_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vmch_hw1_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vmch_hw1_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vmch_hw2_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vmch_hw2_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vmch_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vmch_hw0_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vmch_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vmch_hw1_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vmch_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vmch_hw2_op_cfg(
	void);
extern unsigned int mt6358_upmu_get_da_vmch_mode(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vmch_stbtd(
	unsigned int val);
extern unsigned int mt6358_upmu_get_da_vmch_stb(
	void);
extern unsigned int mt6358_upmu_get_da_vmch_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vmch_ocfb_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vmch_ocfb_en(
	void);
extern unsigned int mt6358_upmu_get_da_vmch_ocfb_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vmch_dummy_load(
	unsigned int val);
extern unsigned int mt6358_upmu_get_da_vmch_dummy_load(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vibr_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vibr_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vibr_lp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vibr_sw_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vibr_sw_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vibr_hw0_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vibr_hw0_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vibr_hw1_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vibr_hw1_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vibr_hw2_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vibr_hw2_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vibr_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vibr_hw0_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vibr_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vibr_hw1_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vibr_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vibr_hw2_op_cfg(
	void);
extern unsigned int mt6358_upmu_get_da_vibr_mode(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vibr_stbtd(
	unsigned int val);
extern unsigned int mt6358_upmu_get_da_vibr_stb(
	void);
extern unsigned int mt6358_upmu_get_da_vibr_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vibr_ocfb_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vibr_ocfb_en(
	void);
extern unsigned int mt6358_upmu_get_da_vibr_ocfb_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vibr_dummy_load(
	unsigned int val);
extern unsigned int mt6358_upmu_get_da_vibr_dummy_load(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcn33_bt_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcn33_bt_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcn33_lp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vcn33_sw_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcn33_sw_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcn33_hw0_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcn33_hw0_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcn33_hw1_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcn33_hw1_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcn33_hw2_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcn33_hw2_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcn33_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcn33_hw0_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcn33_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcn33_hw1_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcn33_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcn33_hw2_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcn33_wifi_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcn33_wifi_en(
	void);
extern unsigned int mt6358_upmu_get_da_vcn33_mode(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcn33_stbtd(
	unsigned int val);
extern unsigned int mt6358_upmu_get_da_vcn33_stb(
	void);
extern unsigned int mt6358_upmu_get_da_vcn33_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcn33_ocfb_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcn33_ocfb_en(
	void);
extern unsigned int mt6358_upmu_get_da_vcn33_ocfb_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcn33_dummy_load(
	unsigned int val);
extern unsigned int mt6358_upmu_get_da_vcn33_dummy_load(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vldo28_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vldo28_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vldo28_lp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vldo28_sw_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vldo28_sw_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vldo28_hw0_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vldo28_hw0_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vldo28_hw1_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vldo28_hw1_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vldo28_hw2_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vldo28_hw2_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vldo28_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vldo28_hw0_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vldo28_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vldo28_hw1_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vldo28_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vldo28_hw2_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vldo28_1_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vldo28_1_en(
	void);
extern unsigned int mt6358_upmu_get_da_vldo28_mode(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vldo28_stbtd(
	unsigned int val);
extern unsigned int mt6358_upmu_get_da_vldo28_stb(
	void);
extern unsigned int mt6358_upmu_get_da_vldo28_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vldo28_ocfb_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vldo28_ocfb_en(
	void);
extern unsigned int mt6358_upmu_get_da_vldo28_ocfb_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vldo28_dummy_load(
	unsigned int val);
extern unsigned int mt6358_upmu_get_da_vldo28_dummy_load(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsim1_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsim1_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsim1_lp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vsim1_sw_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsim1_sw_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsim1_hw0_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsim1_hw0_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsim1_hw1_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsim1_hw1_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsim1_hw2_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsim1_hw2_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsim1_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsim1_hw0_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsim1_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsim1_hw1_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsim1_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsim1_hw2_op_cfg(
	void);
extern unsigned int mt6358_upmu_get_da_vsim1_mode(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsim1_stbtd(
	unsigned int val);
extern unsigned int mt6358_upmu_get_da_vsim1_stb(
	void);
extern unsigned int mt6358_upmu_get_da_vsim1_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsim1_ocfb_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsim1_ocfb_en(
	void);
extern unsigned int mt6358_upmu_get_da_vsim1_ocfb_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsim1_dummy_load(
	unsigned int val);
extern unsigned int mt6358_upmu_get_da_vsim1_dummy_load(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsim2_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsim2_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsim2_lp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vsim2_sw_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsim2_sw_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsim2_hw0_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsim2_hw0_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsim2_hw1_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsim2_hw1_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsim2_hw2_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsim2_hw2_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsim2_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsim2_hw0_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsim2_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsim2_hw1_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsim2_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsim2_hw2_op_cfg(
	void);
extern unsigned int mt6358_upmu_get_da_vsim2_mode(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsim2_stbtd(
	unsigned int val);
extern unsigned int mt6358_upmu_get_da_vsim2_stb(
	void);
extern unsigned int mt6358_upmu_get_da_vsim2_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsim2_ocfb_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsim2_ocfb_en(
	void);
extern unsigned int mt6358_upmu_get_da_vsim2_ocfb_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsim2_dummy_load(
	unsigned int val);
extern unsigned int mt6358_upmu_get_da_vsim2_dummy_load(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vmch_sd_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vmch_sd_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vmch_sd_pol(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vmch_sd_db_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vsim1_sd_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsim1_sd_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsim1_sd_pol(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vsim1_sd_db_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vsim2_sd_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vsim2_sd_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vsim2_sd_pol(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vsim2_sd_db_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_goff2_rsv0(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_goff2_rsv1(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vcn28_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcn28_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcn28_lp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vcn28_sw_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcn28_sw_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcn28_hw0_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcn28_hw0_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcn28_hw1_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcn28_hw1_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcn28_hw2_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcn28_hw2_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcn28_hw3_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcn28_hw3_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcn28_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcn28_hw0_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcn28_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcn28_hw1_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcn28_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcn28_hw2_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcn28_hw3_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcn28_hw3_op_cfg(
	void);
extern unsigned int mt6358_upmu_get_da_vcn28_mode(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcn28_stbtd(
	unsigned int val);
extern unsigned int mt6358_upmu_get_da_vcn28_stb(
	void);
extern unsigned int mt6358_upmu_get_da_vcn28_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcn28_ocfb_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcn28_ocfb_en(
	void);
extern unsigned int mt6358_upmu_get_da_vcn28_ocfb_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcn28_dummy_load(
	unsigned int val);
extern unsigned int mt6358_upmu_get_da_vcn28_dummy_load(
	void);
extern unsigned int mt6358_upmu_set_rg_vrtc28_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vrtc28_en(
	void);
extern unsigned int mt6358_upmu_get_da_vrtc28_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vbif28_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vbif28_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vbif28_lp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_vbif28_sw_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vbif28_sw_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vbif28_hw0_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vbif28_hw0_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vbif28_hw1_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vbif28_hw1_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vbif28_hw2_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vbif28_hw2_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vbif28_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vbif28_hw0_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vbif28_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vbif28_hw1_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vbif28_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vbif28_hw2_op_cfg(
	void);
extern unsigned int mt6358_upmu_get_da_vbif28_mode(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vbif28_stbtd(
	unsigned int val);
extern unsigned int mt6358_upmu_get_da_vbif28_stb(
	void);
extern unsigned int mt6358_upmu_get_da_vbif28_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vbif28_ocfb_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vbif28_ocfb_en(
	void);
extern unsigned int mt6358_upmu_get_da_vbif28_ocfb_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vbif28_dummy_load(
	unsigned int val);
extern unsigned int mt6358_upmu_get_da_vbif28_dummy_load(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_goff3_rsv0(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ldo_goff3_rsv1(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vcama1_vocal(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vcama1_vosel(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vcama1_vosel(
	void);
extern unsigned int mt6358_upmu_set_rg_vcama1_ndis_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vcama1_ndis_en(
	void);
extern unsigned int mt6358_upmu_set_rg_vcama1_rsv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vcama2_vocal(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vcama2_vosel(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vcama2_vosel(
	void);
extern unsigned int mt6358_upmu_set_rg_vcama2_ndis_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vcama2_ndis_en(
	void);
extern unsigned int mt6358_upmu_set_rg_vcama2_rsv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vfe28_vocal(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vfe28_ndis_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vfe28_ndis_en(
	void);
extern unsigned int mt6358_upmu_set_rg_vfe28_rsv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vcn28_vocal(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vcn28_ndis_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vcn28_ndis_en(
	void);
extern unsigned int mt6358_upmu_set_rg_vcn28_rsv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vbif28_vocal(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vbif28_ndis_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vbif28_ndis_en(
	void);
extern unsigned int mt6358_upmu_set_rg_vbif28_rsv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vaud28_vocal(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vaud28_ndis_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vaud28_ndis_en(
	void);
extern unsigned int mt6358_upmu_set_rg_vaud28_rsv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vaux18_vocal(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vaux18_ndis_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vaux18_ndis_en(
	void);
extern unsigned int mt6358_upmu_set_rg_vaux18_rsv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vxo22_vocal(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vxo22_vosel(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vxo22_vosel(
	void);
extern unsigned int mt6358_upmu_set_rg_vxo22_ndis_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vxo22_ndis_en(
	void);
extern unsigned int mt6358_upmu_set_rg_vxo22_rsv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vcn33_vocal(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vcn33_vosel(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vcn33_vosel(
	void);
extern unsigned int mt6358_upmu_set_rg_vcn33_ndis_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vcn33_ndis_en(
	void);
extern unsigned int mt6358_upmu_set_rg_vcn33_rsv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vsim1_vocal(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vsim1_vosel(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vsim1_vosel(
	void);
extern unsigned int mt6358_upmu_set_rg_vsim1_ndis_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vsim1_ndis_en(
	void);
extern unsigned int mt6358_upmu_set_rg_vsim1_rsv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vsim2_vocal(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vsim2_vosel(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vsim2_vosel(
	void);
extern unsigned int mt6358_upmu_set_rg_vsim2_ndis_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vsim2_ndis_en(
	void);
extern unsigned int mt6358_upmu_set_rg_vsim2_rsv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vusb_vocal(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vusb_vosel(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vusb_vosel(
	void);
extern unsigned int mt6358_upmu_set_rg_vusb_ndis_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vusb_ndis_en(
	void);
extern unsigned int mt6358_upmu_set_rg_vusb_rsv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vemc_vocal(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vemc_vosel(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vemc_vosel(
	void);
extern unsigned int mt6358_upmu_set_rg_vemc_ndis_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vemc_ndis_en(
	void);
extern unsigned int mt6358_upmu_set_rg_vemc_rsv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vldo28_vocal(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vldo28_vosel(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vldo28_vosel(
	void);
extern unsigned int mt6358_upmu_set_rg_vldo28_ndis_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vldo28_ndis_en(
	void);
extern unsigned int mt6358_upmu_set_rg_vldo28_rsv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vio28_vocal(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vio28_ndis_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vio28_ndis_en(
	void);
extern unsigned int mt6358_upmu_set_rg_vio28_rsv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vibr_vocal(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vibr_vosel(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vibr_vosel(
	void);
extern unsigned int mt6358_upmu_set_rg_vibr_ndis_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vibr_ndis_en(
	void);
extern unsigned int mt6358_upmu_set_rg_vibr_rsv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vmch_vocal(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vmch_vosel(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vmch_vosel(
	void);
extern unsigned int mt6358_upmu_set_rg_vmch_ndis_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vmch_ndis_en(
	void);
extern unsigned int mt6358_upmu_set_rg_vmch_stb_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vmch_rsv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vmc_vocal(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vmc_vosel(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vmc_vosel(
	void);
extern unsigned int mt6358_upmu_set_rg_vmc_ndis_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vmc_ndis_en(
	void);
extern unsigned int mt6358_upmu_set_rg_vmc_stb_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vmc_rsv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vcama1_votrim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vcama2_votrim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vfe28_votrim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vcn28_votrim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vbif28_votrim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vaud28_votrim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vaux18_votrim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vxo22_votrim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vcn33_votrim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vcn33_oc_trim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vsim1_votrim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vsim2_votrim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vusb_votrim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vemc_votrim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vemc_oc_trim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vldo28_votrim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vio28_votrim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vibr_votrim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vmch_votrim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vmch_oc_trim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vmc_votrim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vrf18_vocal(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vrf18_ndis_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vrf18_ndis_en(
	void);
extern unsigned int mt6358_upmu_set_rg_vrf18_rsv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vcn18_vocal(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vcn18_ndis_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vcn18_ndis_en(
	void);
extern unsigned int mt6358_upmu_set_rg_vcn18_rsv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vcamio_vocal(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vcamio_ndis_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vcamio_ndis_en(
	void);
extern unsigned int mt6358_upmu_set_rg_vcamio_rsv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vio18_vocal(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vio18_ndis_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vio18_ndis_en(
	void);
extern unsigned int mt6358_upmu_set_rg_vio18_rsv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vefuse_vocal(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vefuse_vosel(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vefuse_vosel(
	void);
extern unsigned int mt6358_upmu_set_rg_vefuse_ndis_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vefuse_ndis_en(
	void);
extern unsigned int mt6358_upmu_set_rg_vefuse_rsv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vrf12_vocal(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vrf12_ndis_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vrf12_ndis_en(
	void);
extern unsigned int mt6358_upmu_set_rg_vrf12_rsv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vsram_proc11_stb_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vsram_proc11_ndis_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vsram_proc11_ndis_en(
	void);
extern unsigned int mt6358_upmu_set_rg_vsram_proc11_ndis_plcur(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vsram_proc11_plcur_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vsram_proc11_plcur_en(
	void);
extern unsigned int mt6358_upmu_set_rg_vsram_proc11_lpoc_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vsram_proc11_lpoc_en(
	void);
extern unsigned int mt6358_upmu_set_rg_vsram_proc11_rsv_h(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vsram_proc11_rsv_l(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vsram_proc11_auto_track(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vsram_proc12_stb_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vsram_proc12_ndis_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vsram_proc12_ndis_en(
	void);
extern unsigned int mt6358_upmu_set_rg_vsram_proc12_ndis_plcur(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vsram_proc12_plcur_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vsram_proc12_plcur_en(
	void);
extern unsigned int mt6358_upmu_set_rg_vsram_proc12_lpoc_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vsram_proc12_lpoc_en(
	void);
extern unsigned int mt6358_upmu_set_rg_vsram_proc12_rsv_h(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vsram_proc12_rsv_l(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vsram_others_stb_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vsram_others_ndis_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vsram_others_ndis_en(
	void);
extern unsigned int mt6358_upmu_set_rg_vsram_others_ndis_plcur(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vsram_others_plcur_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vsram_others_plcur_en(
	void);
extern unsigned int mt6358_upmu_set_rg_vsram_others_lpoc_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vsram_others_lpoc_en(
	void);
extern unsigned int mt6358_upmu_set_rg_vsram_others_rsv_h(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vsram_others_rsv_l(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vsram_gpu_stb_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vsram_gpu_ndis_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vsram_gpu_ndis_en(
	void);
extern unsigned int mt6358_upmu_set_rg_vsram_gpu_ndis_plcur(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vsram_gpu_plcur_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vsram_gpu_plcur_en(
	void);
extern unsigned int mt6358_upmu_set_rg_vsram_gpu_lpoc_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vsram_gpu_lpoc_en(
	void);
extern unsigned int mt6358_upmu_set_rg_vsram_gpu_rsv_h(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vsram_gpu_rsv_l(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vdram2_vocal(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vdram2_ndis_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vdram2_ndis_en(
	void);
extern unsigned int mt6358_upmu_set_rg_vdram2_rsv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vcamd_vocal(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vcamd_vosel(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vcamd_vosel(
	void);
extern unsigned int mt6358_upmu_set_rg_vcamd_ndis_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vcamd_ndis_en(
	void);
extern unsigned int mt6358_upmu_set_rg_vcamd_rsv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_va12_vocal(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_va12_ndis_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_va12_ndis_en(
	void);
extern unsigned int mt6358_upmu_set_rg_va12_rsv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vrf18_votrim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vcn18_votrim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vcamio_votrim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vio18_votrim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vefuse_votrim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vrf12_votrim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vdram2_votrim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vcamd_votrim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_va12_votrim(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vrtc28_bias_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_isink_trim_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_isink_trim_en(
	void);
extern unsigned int mt6358_upmu_set_rg_isink_trim_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_isink_rsv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_isink0_chop_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_isink0_chop_en(
	void);
extern unsigned int mt6358_upmu_set_rg_isink1_chop_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_isink1_chop_en(
	void);
extern unsigned int mt6358_upmu_set_rg_isink0_double(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_isink1_double(
	unsigned int val);
extern unsigned int mt6358_upmu_set_isink0_rsv1(
	unsigned int val);
extern unsigned int mt6358_upmu_set_isink0_rsv0(
	unsigned int val);
extern unsigned int mt6358_upmu_set_isink_ch0_step(
	unsigned int val);
extern unsigned int mt6358_upmu_set_isink1_rsv1(
	unsigned int val);
extern unsigned int mt6358_upmu_set_isink1_rsv0(
	unsigned int val);
extern unsigned int mt6358_upmu_set_isink_ch1_step(
	unsigned int val);
extern unsigned int mt6358_upmu_get_ad_isink0_status(
	void);
extern unsigned int mt6358_upmu_get_ad_isink1_status(
	void);
extern unsigned int mt6358_upmu_set_isink_ch1_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_isink_ch0_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_isink_chop1_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_isink_chop0_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_isink_ch1_bias_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_isink_ch0_bias_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_isink_trim_bias(
	unsigned int val);
extern unsigned int mt6358_upmu_get_aud_top_ana_id(
	void);
extern unsigned int mt6358_upmu_get_aud_top_dig_id(
	void);
extern unsigned int mt6358_upmu_get_aud_top_ana_minor_rev(
	void);
extern unsigned int mt6358_upmu_get_aud_top_ana_major_rev(
	void);
extern unsigned int mt6358_upmu_get_aud_top_dig_minor_rev(
	void);
extern unsigned int mt6358_upmu_get_aud_top_dig_major_rev(
	void);
extern unsigned int mt6358_upmu_get_aud_top_clk_offset(
	void);
extern unsigned int mt6358_upmu_get_aud_top_rst_offset(
	void);
extern unsigned int mt6358_upmu_get_aud_top_int_offset(
	void);
extern unsigned int mt6358_upmu_get_aud_top_int_len(
	void);
extern unsigned int mt6358_upmu_set_rg_accdet_ck_pdn(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_aud_ck_pdn(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audif_ck_pdn(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_zcd13m_ck_pdn(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audncp_ck_pdn(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_pad_aud_clk_miso_ck_pdn(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_aud_intrp_ck_pdn(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vow32k_ck_pdn(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vow13m_ck_pdn(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_aud_ck_cksel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audif_ck_cksel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_aud26m_ck_tst_dis(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_aud_ck_tstsel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audif_ck_tstsel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_aud26m_ck_tstsel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vow13m_ck_tst_dis(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vow13m_ck_tstsel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_aud_intrp_ck_pdn_hwen(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audio_rst(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_accdet_rst(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_zcd_rst(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audncp_rst(
	unsigned int val);
extern unsigned int mt6358_upmu_set_bank_accdet_swrst(
	unsigned int val);
extern unsigned int mt6358_upmu_set_bank_audio_swrst(
	unsigned int val);
extern unsigned int mt6358_upmu_set_bank_audzcd_swrst(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_en_audio(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_audio(
	void);
extern unsigned int mt6358_upmu_set_rg_int_en_accdet(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_accdet(
	void);
extern unsigned int mt6358_upmu_set_rg_int_en_accdet_eint0(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_accdet_eint0(
	void);
extern unsigned int mt6358_upmu_set_rg_int_en_accdet_eint1(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_en_accdet_eint1(
	void);
extern unsigned int mt6358_upmu_set_rg_int_mask_audio(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_accdet(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_accdet_eint0(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_int_mask_accdet_eint1(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_audio(
	void);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_accdet(
	void);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_accdet_eint0(
	void);
extern unsigned int mt6358_upmu_get_rg_int_raw_status_accdet_eint1(
	void);
extern unsigned int mt6358_upmu_set_rg_aud_top_int_polarity(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_divcks_chg(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_divcks_on(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_divcks_prg(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_divcks_pwd_ncp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_divcks_pwd_ncp_st_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_aud_top_mon_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_aud_clk_int_mon_flag_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_aud_clk_int_mon_flag_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_aud_clk_int_mon_flag_en(
	void);
extern unsigned int mt6358_upmu_get_audio_dig_ana_id(
	void);
extern unsigned int mt6358_upmu_get_audio_dig_dig_id(
	void);
extern unsigned int mt6358_upmu_get_audio_dig_ana_minor_rev(
	void);
extern unsigned int mt6358_upmu_get_audio_dig_ana_major_rev(
	void);
extern unsigned int mt6358_upmu_get_audio_dig_dig_minor_rev(
	void);
extern unsigned int mt6358_upmu_get_audio_dig_dig_major_rev(
	void);
extern unsigned int mt6358_upmu_set_afe_on(
	unsigned int val);
extern unsigned int mt6358_upmu_set_afe_dl_lr_swap(
	unsigned int val);
extern unsigned int mt6358_upmu_set_afe_ul_lr_swap(
	unsigned int val);
extern unsigned int mt6358_upmu_set_dl_2_src_on_tmp_ctl_pre(
	unsigned int val);
extern unsigned int mt6358_upmu_set_c_two_digital_mic_ctl(
	unsigned int val);
extern unsigned int mt6358_upmu_set_c_digmic_phase_sel_ch2_ctl(
	unsigned int val);
extern unsigned int mt6358_upmu_set_c_digmic_phase_sel_ch1_ctl(
	unsigned int val);
extern unsigned int mt6358_upmu_set_ul_src_on_tmp_ctl(
	unsigned int val);
extern unsigned int mt6358_upmu_set_ul_sdm_3_level_ctl(
	unsigned int val);
extern unsigned int mt6358_upmu_set_ul_loop_back_mode_ctl(
	unsigned int val);
extern unsigned int mt6358_upmu_set_digmic_3p25m_1p625m_sel_ctl(
	unsigned int val);
extern unsigned int mt6358_upmu_set_dmic_low_power_mode_ctl(
	unsigned int val);
extern unsigned int mt6358_upmu_set_dl_sine_on(
	unsigned int val);
extern unsigned int mt6358_upmu_set_ul_sine_on(
	unsigned int val);
extern unsigned int mt6358_upmu_set_mtkaif_sine_on(
	unsigned int val);
extern unsigned int mt6358_upmu_set_pdn_reserved(
	unsigned int val);
extern unsigned int mt6358_upmu_set_pdn_afe_testmodel_ctl(
	unsigned int val);
extern unsigned int mt6358_upmu_set_pwr_clk_dis_ctl(
	unsigned int val);
extern unsigned int mt6358_upmu_set_pdn_i2s_dl_ctl(
	unsigned int val);
extern unsigned int mt6358_upmu_set_pdn_adc_ctl(
	unsigned int val);
extern unsigned int mt6358_upmu_set_pdn_dac_ctl(
	unsigned int val);
extern unsigned int mt6358_upmu_set_pdn_afe_ctl(
	unsigned int val);
extern unsigned int mt6358_upmu_set_afe_mon_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_audio_sys_top_mon_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_audio_sys_top_mon_swap(
	unsigned int val);
extern unsigned int mt6358_upmu_set_cci_scrambler_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_cci_aud_sdm_7bit_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_cci_aud_sdm_muter(
	unsigned int val);
extern unsigned int mt6358_upmu_set_cci_aud_sdm_mutel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_cci_aud_split_test_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_cci_zero_pad_disable(
	unsigned int val);
extern unsigned int mt6358_upmu_set_cci_aud_idac_test_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_cci_splt_scrmb_on(
	unsigned int val);
extern unsigned int mt6358_upmu_set_cci_splt_scrmb_clk_on(
	unsigned int val);
extern unsigned int mt6358_upmu_set_cci_rand_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_cci_lch_inv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_cci_scrambler_cg_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_cci_audio_fifo_wptr(
	unsigned int val);
extern unsigned int mt6358_upmu_set_cci_aud_anack_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_aud_sdm_test_r(
	unsigned int val);
extern unsigned int mt6358_upmu_set_aud_sdm_test_l(
	unsigned int val);
extern unsigned int mt6358_upmu_set_cci_acd_func_rstb(
	unsigned int val);
extern unsigned int mt6358_upmu_set_cci_afifo_clk_pwdb(
	unsigned int val);
extern unsigned int mt6358_upmu_set_cci_acd_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_cci_audio_fifo_enable(
	unsigned int val);
extern unsigned int mt6358_upmu_set_cci_audio_fifo_clkin_inv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_cci_aud_dac_ana_rstb_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_cci_aud_dac_ana_mute(
	unsigned int val);
extern unsigned int mt6358_upmu_set_digmic_testck_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_digmic_testck_src_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_sdm_testck_src_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_sdm_ana13m_testck_src_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_sdm_ana13m_testck_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_ul_fifo_wclk_6p5m_testck_src_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_ul_fifo_wclk_6p5m_testck_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_ul_fifo_wdata_testsrc_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_ul_fifo_wdata_testen(
	unsigned int val);
extern unsigned int mt6358_upmu_set_ul_fifo_digmic_wdata_testsrc_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_ul_fifo_wclk_inv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_r_aud_dac_neg_large_mono(
	unsigned int val);
extern unsigned int mt6358_upmu_set_r_aud_dac_pos_large_mono(
	unsigned int val);
extern unsigned int mt6358_upmu_set_r_aud_dac_sw_rstb(
	unsigned int val);
extern unsigned int mt6358_upmu_set_r_aud_dac_mono_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_r_aud_dac_neg_tiny_mono(
	unsigned int val);
extern unsigned int mt6358_upmu_set_r_aud_dac_pos_tiny_mono(
	unsigned int val);
extern unsigned int mt6358_upmu_set_r_aud_dac_neg_small_mono(
	unsigned int val);
extern unsigned int mt6358_upmu_set_r_aud_dac_pos_small_mono(
	unsigned int val);
extern unsigned int mt6358_upmu_get_aud_scr_out_r(
	void);
extern unsigned int mt6358_upmu_get_aud_scr_out_l(
	void);
extern unsigned int mt6358_upmu_set_rg_mtkaif_rxif_fifo_inten(
	unsigned int val);
extern unsigned int mt6358_upmu_set_afe_reserved(
	unsigned int val);
extern unsigned int mt6358_upmu_get_mtkaif_rxif_rd_empty_status(
	void);
extern unsigned int mt6358_upmu_get_mtkaif_rxif_wr_full_status(
	void);
extern unsigned int mt6358_upmu_get_mtkaif_rxif_fifo_status(
	void);
extern unsigned int mt6358_upmu_get_mtkaiftx_v3_sdata_out1(
	void);
extern unsigned int mt6358_upmu_get_mtkaiftx_v3_sdata_out2(
	void);
extern unsigned int mt6358_upmu_get_mtkaiftx_v3_sync_out(
	void);
extern unsigned int mt6358_upmu_get_mtkaif_rxif_invalid_cycle(
	void);
extern unsigned int mt6358_upmu_get_mtkaif_rxif_invalid_flag(
	void);
extern unsigned int mt6358_upmu_get_mtkaif_rxif_search_fail_flag(
	void);
extern unsigned int mt6358_upmu_get_mtkaifrx_v3_sdata_in1(
	void);
extern unsigned int mt6358_upmu_get_mtkaifrx_v3_sdata_in2(
	void);
extern unsigned int mt6358_upmu_get_mtkaifrx_v3_sync_in(
	void);
extern unsigned int mt6358_upmu_get_mtkaif_txif_in_ch1(
	void);
extern unsigned int mt6358_upmu_get_mtkaif_txif_in_ch2(
	void);
extern unsigned int mt6358_upmu_get_mtkaif_rxif_out_ch1(
	void);
extern unsigned int mt6358_upmu_get_mtkaif_rxif_out_ch2(
	void);
extern unsigned int mt6358_upmu_set_rg_mtkaif_loopback_test1(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_mtkaif_loopback_test2(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_mtkaif_pmic_txif_8to5(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_mtkaif_txif_protocol2(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_mtkaif_bypass_src_test(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_mtkaif_bypass_src_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_mtkaif_rxif_protocol2(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_mtkaif_rxif_clkinv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_mtkaif_rxif_data_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_mtkaif_rxif_detect_on(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_mtkaif_rxif_fifo_rsp(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_mtkaif_rxif_data_bit(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_mtkaif_rxif_voice_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_mtkaif_rxif_voice_mode_protocol2(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_mtkaif_rxif_sync_check_round(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_mtkaif_rxif_invalid_sync_check_round(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_mtkaif_rxif_sync_search_table(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_mtkaif_rxif_sync_cnt_table(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_mtkaif_rxif_clear_sync_fail(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_mtkaif_rxif_detect_on_protocol2(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_mtkaif_rxif_fifo_rsp_protocol2(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_mtkaif_rxif_loopback_use_nle(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_mtkaif_sync_word1(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_mtkaif_sync_word2(
	unsigned int val);
extern unsigned int mt6358_upmu_set_r_aud_sdm_mute_r(
	unsigned int val);
extern unsigned int mt6358_upmu_set_r_aud_sdm_mute_l(
	unsigned int val);
extern unsigned int mt6358_upmu_set_c_mute_sw_ctl(
	unsigned int val);
extern unsigned int mt6358_upmu_set_c_dac_en_ctl(
	unsigned int val);
extern unsigned int mt6358_upmu_set_c_amp_div_ch1_ctl(
	unsigned int val);
extern unsigned int mt6358_upmu_set_c_freq_div_ch1_ctl(
	unsigned int val);
extern unsigned int mt6358_upmu_set_c_sgen_rch_inv_8bit(
	unsigned int val);
extern unsigned int mt6358_upmu_set_c_sgen_rch_inv_5bit(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_amic_ul_adc_clk_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ul_async_fifo_soft_rst(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_ul_async_fifo_soft_rst_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ul_async_fifo_soft_rst_en(
	void);
extern unsigned int mt6358_upmu_set_dcclk_gen_on(
	unsigned int val);
extern unsigned int mt6358_upmu_set_dcclk_pdn(
	unsigned int val);
extern unsigned int mt6358_upmu_set_dcclk_inv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_dcclk_div(
	unsigned int val);
extern unsigned int mt6358_upmu_set_dcclk_phase_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_dcclk_resync_bypass(
	unsigned int val);
extern unsigned int mt6358_upmu_set_resync_src_ck_inv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_resync_src_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_aud_pad_top_phase_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_aud_pad_top_dat_miso_loopback(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_aud_pad_top_phase_mode2(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_aud_pad_top_dat_miso2_loopback(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_aud_pad_top_tx_fifo_on(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_aud_pad_top_mtkaif_clk_protocol2(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_aud_pad_top_tx_fifo_rsp(
	unsigned int val);
extern unsigned int mt6358_upmu_get_adda_aud_pad_top_mon(
	void);
extern unsigned int mt6358_upmu_get_adda_aud_pad_top_mon1(
	void);
extern unsigned int mt6358_upmu_set_nle_lch_on(
	unsigned int val);
extern unsigned int mt6358_upmu_set_nle_lch_ch_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_nle_lch_hpgain_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_nle_rch_on(
	unsigned int val);
extern unsigned int mt6358_upmu_set_nle_rch_ch_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_nle_rch_hpgain_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_get_nle_monitor(
	void);
extern unsigned int mt6358_upmu_get_ck_cg_en_mon(
	void);
extern unsigned int mt6358_upmu_get_audio_dig_2nd_ana_id(
	void);
extern unsigned int mt6358_upmu_get_audio_dig_2nd_dig_id(
	void);
extern unsigned int mt6358_upmu_get_audio_dig_2nd_ana_minor_rev(
	void);
extern unsigned int mt6358_upmu_get_audio_dig_2nd_ana_major_rev(
	void);
extern unsigned int mt6358_upmu_get_audio_dig_2nd_dig_minor_rev(
	void);
extern unsigned int mt6358_upmu_get_audio_dig_2nd_dig_major_rev(
	void);
extern unsigned int mt6358_upmu_set_rg_up8x_sync_word(
	unsigned int val);
extern unsigned int mt6358_upmu_get_vow_intr_flag(
	void);
extern unsigned int mt6358_upmu_set_sample_base_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_s_n_value_rst(
	unsigned int val);
extern unsigned int mt6358_upmu_set_vow_intr_clr(
	unsigned int val);
extern unsigned int mt6358_upmu_set_vow_intr_source_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_vow_loop_back_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_vow_sdm_3_level(
	unsigned int val);
extern unsigned int mt6358_upmu_set_main_dmic_ck_vow_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_vow_digmic_ck_phase_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_vow_on(
	unsigned int val);
extern unsigned int mt6358_upmu_set_vow_ck_div_rst(
	unsigned int val);
extern unsigned int mt6358_upmu_set_vow_digmic_on(
	unsigned int val);
extern unsigned int mt6358_upmu_set_vow_1p6m_800k_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_pdn_vow(
	unsigned int val);
extern unsigned int mt6358_upmu_set_ampref(
	unsigned int val);
extern unsigned int mt6358_upmu_set_timerini(
	unsigned int val);
extern unsigned int mt6358_upmu_set_a_ini(
	unsigned int val);
extern unsigned int mt6358_upmu_set_b_ini(
	unsigned int val);
extern unsigned int mt6358_upmu_set_a_default(
	unsigned int val);
extern unsigned int mt6358_upmu_set_b_default(
	unsigned int val);
extern unsigned int mt6358_upmu_set_k_alpha_fall(
	unsigned int val);
extern unsigned int mt6358_upmu_set_k_alpha_rise(
	unsigned int val);
extern unsigned int mt6358_upmu_set_k_beta_fall(
	unsigned int val);
extern unsigned int mt6358_upmu_set_k_beta_rise(
	unsigned int val);
extern unsigned int mt6358_upmu_set_k_gamma(
	unsigned int val);
extern unsigned int mt6358_upmu_set_vow_txif_sck_div(
	unsigned int val);
extern unsigned int mt6358_upmu_set_vow_txif_mono(
	unsigned int val);
extern unsigned int mt6358_upmu_set_vow_adc_clk_inv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_vow_adc_testck_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_vow_adc_testck_src_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_vow_txif_sck_inv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_n_min(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_dvfs_hw_cnt_thr(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_dvfs_done_hw_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_buck_dvfs_done_sw_ctl(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_flr_ratio(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_flr_bypass(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_window_size_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_get_vow_downcnt(
	void);
extern unsigned int mt6358_upmu_get_second_cnt_start(
	void);
extern unsigned int mt6358_upmu_get_vow_a(
	void);
extern unsigned int mt6358_upmu_get_vow_b(
	void);
extern unsigned int mt6358_upmu_get_slt_counter_mon(
	void);
extern unsigned int mt6358_upmu_get_k_tmp_mon(
	void);
extern unsigned int mt6358_upmu_get_vow_s_l(
	void);
extern unsigned int mt6358_upmu_get_vow_s_h(
	void);
extern unsigned int mt6358_upmu_get_vow_n_l(
	void);
extern unsigned int mt6358_upmu_get_vow_n_h(
	void);
extern unsigned int mt6358_upmu_set_vow_sn_ini_cfg_val(
	unsigned int val);
extern unsigned int mt6358_upmu_set_vow_sn_ini_cfg_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_vow_tgen_freq_div(
	unsigned int val);
extern unsigned int mt6358_upmu_set_vow_tgen_mute_sw(
	unsigned int val);
extern unsigned int mt6358_upmu_set_vow_tgen_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_vow_posdiv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_vow_cic_mode_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_vow_intr_sw_val(
	unsigned int val);
extern unsigned int mt6358_upmu_set_vow_intr_sw_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_vow_clk_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_a1p6m_en_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_a1p6m_en_sel(
	void);
extern unsigned int mt6358_upmu_set_rg_buck_clk_div(
	unsigned int val);
extern unsigned int mt6358_upmu_set_vow_32k_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_get_buck_dvfs_done(
	void);
extern unsigned int mt6358_upmu_set_rg_hpf_on(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_snrdet_hpf_bypass(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_mtkaif_hpf_bypass(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_baseline_alpha_order(
	unsigned int val);
extern unsigned int mt6358_upmu_set_vow_posdivclk_pdn(
	unsigned int val);
extern unsigned int mt6358_upmu_set_vow_dmicclk_pdn(
	unsigned int val);
extern unsigned int mt6358_upmu_set_vow_irq_latch_snr_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_vow_hpf_dc_test(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_periodic_cnt_period(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_periodic_cnt_clr(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_periodic_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_periodic_en(
	void);
extern unsigned int mt6358_upmu_set_rg_periodic_cnt_set_value(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_periodic_cnt_pause(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_periodic_cnt_set(
	unsigned int val);
extern unsigned int mt6358_upmu_set_audpreamplon_periodic_on_cycle(
	unsigned int val);
extern unsigned int mt6358_upmu_set_audpreamplon_periodic_inverse(
	unsigned int val);
extern unsigned int mt6358_upmu_set_audpreamplon_periodic_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_audpreampldcprecharge_periodic_on_cycle(
	unsigned int val);
extern unsigned int mt6358_upmu_set_audpreampldcprecharge_periodic_inverse(
	unsigned int val);
extern unsigned int mt6358_upmu_set_audpreampldcprecharge_periodic_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_audadclpwrup_periodic_on_cycle(
	unsigned int val);
extern unsigned int mt6358_upmu_set_audadclpwrup_periodic_inverse(
	unsigned int val);
extern unsigned int mt6358_upmu_set_audadclpwrup_periodic_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_audglbvowlpwen_periodic_on_cycle(
	unsigned int val);
extern unsigned int mt6358_upmu_set_audglbvowlpwen_periodic_inverse(
	unsigned int val);
extern unsigned int mt6358_upmu_set_audglbvowlpwen_periodic_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auddigmicen_periodic_on_cycle(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auddigmicen_periodic_inverse(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auddigmicen_periodic_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_audpwdbmicbias0_periodic_on_cycle(
	unsigned int val);
extern unsigned int mt6358_upmu_set_audpwdbmicbias0_periodic_inverse(
	unsigned int val);
extern unsigned int mt6358_upmu_set_audpwdbmicbias0_periodic_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_audpwdbmicbias1_periodic_on_cycle(
	unsigned int val);
extern unsigned int mt6358_upmu_set_audpwdbmicbias1_periodic_inverse(
	unsigned int val);
extern unsigned int mt6358_upmu_set_audpwdbmicbias1_periodic_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_vow_ck_en_periodic_on_cycle(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_vow_ck_en_periodic_inverse(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_vow_ck_en_periodic_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_audglb_pwrdn_periodic_on_cycle(
	unsigned int val);
extern unsigned int mt6358_upmu_set_audglb_pwrdn_periodic_inverse(
	unsigned int val);
extern unsigned int mt6358_upmu_set_audglb_pwrdn_periodic_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_vow_on_periodic_on_cycle(
	unsigned int val);
extern unsigned int mt6358_upmu_set_vow_on_periodic_inverse(
	unsigned int val);
extern unsigned int mt6358_upmu_set_vow_on_periodic_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_dmic_on_periodic_on_cycle(
	unsigned int val);
extern unsigned int mt6358_upmu_set_dmic_on_periodic_inverse(
	unsigned int val);
extern unsigned int mt6358_upmu_set_dmic_on_periodic_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_audpreamplon_periodic_off_cycle(
	unsigned int val);
extern unsigned int mt6358_upmu_set_pdn_vow_f32k_ck(
	unsigned int val);
extern unsigned int mt6358_upmu_set_audpreampldcprecharge_periodic_off_cycle(
	unsigned int val);
extern unsigned int mt6358_upmu_set_vow_snrdet_periodic_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_set_audadclpwrup_periodic_off_cycle(
	unsigned int val);
extern unsigned int mt6358_upmu_set_audglbvowlpwen_periodic_off_cycle(
	unsigned int val);
extern unsigned int mt6358_upmu_set_auddigmicen_periodic_off_cycle(
	unsigned int val);
extern unsigned int mt6358_upmu_set_audpwdbmicbias0_periodic_off_cycle(
	unsigned int val);
extern unsigned int mt6358_upmu_set_audpwdbmicbias1_periodic_off_cycle(
	unsigned int val);
extern unsigned int mt6358_upmu_set_xo_vow_ck_en_periodic_off_cycle(
	unsigned int val);
extern unsigned int mt6358_upmu_set_clksq_en_vow_periodic_mode(
	unsigned int val);
extern unsigned int mt6358_upmu_set_audglb_pwrdn_periodic_off_cycle(
	unsigned int val);
extern unsigned int mt6358_upmu_set_vow_on_periodic_off_cycle(
	unsigned int val);
extern unsigned int mt6358_upmu_set_dmic_on_periodic_off_cycle(
	unsigned int val);
extern unsigned int mt6358_upmu_get_vow_periodic_mon(
	void);
extern unsigned int mt6358_upmu_get_vow_periodic_count_mon(
	void);
extern unsigned int mt6358_upmu_get_audenc_ana_id(
	void);
extern unsigned int mt6358_upmu_get_audenc_dig_id(
	void);
extern unsigned int mt6358_upmu_get_audenc_ana_minor_rev(
	void);
extern unsigned int mt6358_upmu_get_audenc_ana_major_rev(
	void);
extern unsigned int mt6358_upmu_get_audenc_dig_minor_rev(
	void);
extern unsigned int mt6358_upmu_get_audenc_dig_major_rev(
	void);
extern unsigned int mt6358_upmu_set_rg_audpreamplon(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audpreampldccen(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audpreampldcprecharge(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audpreamplpgatest(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audpreamplvscale(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audpreamplinputsel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audpreamplgain(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audadclpwrup(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audadclinputsel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audpreampron(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audpreamprdccen(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audpreamprdcprecharge(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audpreamprpgatest(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audpreamprvscale(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audpreamprinputsel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audpreamprgain(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audio_vow_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_audio_vow_en(
	void);
extern unsigned int mt6358_upmu_set_rg_audadcrpwrup(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audadcrinputsel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_clksq_en_vow(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_clksq_en_vow(
	void);
extern unsigned int mt6358_upmu_set_rg_audulhalfbias(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audglbvowlpwen(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audpreamplpen(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audadc1ststagelpen(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audadc2ndstagelpen(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audadcflashlpen(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audpreampiddtest(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audadc1ststageiddtest(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audadc2ndstageiddtest(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audadcrefbufiddtest(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audadcflashiddtest(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audadcdac0p25fs(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audadcclksel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audadcclksource(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audpreampaafen(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_dccvcmbuflpmodsel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_dccvcmbuflpswen(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_cmstbenh(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_pgabodysw(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audadc1ststagesdenb(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audadc2ndstagereset(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audadc3rdstagereset(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audadcfsreset(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audadcwidecm(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audadcnopatest(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audadcbypass(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audadcffbypass(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audadcdacfbcurrent(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audadcdaciddtest(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audadcdacnrz(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audadcnodem(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audadcdactest(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audrctunel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audrctunelsel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audrctuner(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audrctunersel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_clksq_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_clksq_en(
	void);
extern unsigned int mt6358_upmu_set_rg_clksq_in_sel_test(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_cm_refgensel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audspare(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audencspare(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audencspare2(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_auddigmicen(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_auddigmicbias(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_dmichpclken(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_auddigmicpduty(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_auddigmicnduty(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_dmicmonen(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_dmicmonsel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audsparevmic(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audpwdbmicbias0(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audmicbias0bypassen(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audmicbias0lowpen(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audmicbias0vref(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audmicbias0dcsw0p1en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audmicbias0dcsw0p2en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audmicbias0dcsw0nen(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audmicbias0dcsw2p1en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audmicbias0dcsw2p2en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audmicbias0dcsw2nen(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audpwdbmicbias1(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audmicbias1bypassen(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audmicbias1lowpen(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audmicbias1vref(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audmicbias1dcsw1pen(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audmicbias1dcsw1nen(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_bandgapgen(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_mtest_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_mtest_en(
	void);
extern unsigned int mt6358_upmu_set_rg_mtest_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_mtest_current(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audaccdetmicbias0pulllow(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audaccdetmicbias1pulllow(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audaccdetvin1pulllow(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audaccdetvthacal(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audaccdetvthbcal(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audaccdettvdet(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_accdetsel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_swbufmodsel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_swbufswen(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_eintcompvth(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_eintconfigaccdet(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_einthirenb(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_accdet2auxresbypass(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_accdet2auxbufferbypass(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_accdet2auxswen(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rgs_audrctunelread(
	void);
extern unsigned int mt6358_upmu_get_rgs_audrctunerread(
	void);
extern unsigned int mt6358_upmu_get_auddec_ana_id(
	void);
extern unsigned int mt6358_upmu_get_auddec_dig_id(
	void);
extern unsigned int mt6358_upmu_get_auddec_ana_minor_rev(
	void);
extern unsigned int mt6358_upmu_get_auddec_ana_major_rev(
	void);
extern unsigned int mt6358_upmu_get_auddec_dig_minor_rev(
	void);
extern unsigned int mt6358_upmu_get_auddec_dig_major_rev(
	void);
extern unsigned int mt6358_upmu_set_rg_auddaclpwrup_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_auddacrpwrup_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_aud_dac_pwr_up_va28(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_aud_dac_pwl_up_va28(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audhplpwrup_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audhprpwrup_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audhplpwrup_ibias_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audhprpwrup_ibias_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audhplmuxinputsel_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audhprmuxinputsel_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audhplscdisable_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audhprscdisable_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audhplbsccurrent_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audhprbsccurrent_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audhploutpwrup_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audhproutpwrup_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audhploutauxpwrup_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audhproutauxpwrup_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_hplauxfbrsw_en_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_hplauxfbrsw_en_vaudp15(
	void);
extern unsigned int mt6358_upmu_set_rg_hprauxfbrsw_en_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_hprauxfbrsw_en_vaudp15(
	void);
extern unsigned int mt6358_upmu_set_rg_hplshort2hplaux_en_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_hplshort2hplaux_en_vaudp15(
	void);
extern unsigned int mt6358_upmu_set_rg_hprshort2hpraux_en_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_hprshort2hpraux_en_vaudp15(
	void);
extern unsigned int mt6358_upmu_set_rg_hploutstgctrl_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_hproutstgctrl_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_hploutputstbenh_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_hproutputstbenh_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audhpstartup_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audrefn_deres_en_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_audrefn_deres_en_vaudp15(
	void);
extern unsigned int mt6358_upmu_set_rg_hppshort2vcm_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_hpinputstbenh_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_hpinputreset0_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_hpoutputreset0_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_abidec_rsvd0_vaudp28(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audhpdecmgainadj_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audhpdedmgainadj_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audhspwrup_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audhspwrup_ibias_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audhsmuxinputsel_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audhsscdisable_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audhsbsccurrent_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audhsstartup_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_hsoutputstbenh_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_hsinputstbenh_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_hsinputreset0_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_hsoutputreset0_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_hsout_shortvcm_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audlolpwrup_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audlolpwrup_ibias_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audlolmuxinputsel_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audlolscdisable_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audlolbsccurrent_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audlostartup_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_loinputstbenh_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_looutputstbenh_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_loinputreset0_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_looutputreset0_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_loout_shortvcm_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audtrimbuf_inputmuxsel_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audtrimbuf_gainsel_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audtrimbuf_en_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_audtrimbuf_en_vaudp15(
	void);
extern unsigned int mt6358_upmu_set_rg_audhpspkdet_inputmuxsel_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audhpspkdet_outputmuxsel_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audhpspkdet_en_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_audhpspkdet_en_vaudp15(
	void);
extern unsigned int mt6358_upmu_set_rg_abidec_rsvd0_va28(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_abidec_rsvd0_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_abidec_rsvd1_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_abidec_rsvd2_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audzcdmuxsel_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audzcdclksel_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audbiasadj_0_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audbiasadj_1_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audibiaspwrdn_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_rstb_decoder_va28(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_sel_decoder_96k_va28(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_sel_delay_vcore(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audglb_pwrdn_va28(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_rstb_encoder_va28(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_rstb_encoder_va28(
	void);
extern unsigned int mt6358_upmu_set_rg_sel_encoder_96k_va28(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_sel_encoder_96k_va28(
	void);
extern unsigned int mt6358_upmu_set_rg_hcldo_en_va18(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_hcldo_en_va18(
	void);
extern unsigned int mt6358_upmu_set_rg_hcldo_pddis_en_va18(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_hcldo_pddis_en_va18(
	void);
extern unsigned int mt6358_upmu_set_rg_hcldo_remote_sense_va18(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_lcldo_en_va18(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_lcldo_en_va18(
	void);
extern unsigned int mt6358_upmu_set_rg_lcldo_pddis_en_va18(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_lcldo_pddis_en_va18(
	void);
extern unsigned int mt6358_upmu_set_rg_lcldo_remote_sense_va18(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_lcldo_enc_en_va28(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_lcldo_enc_en_va28(
	void);
extern unsigned int mt6358_upmu_set_rg_lcldo_enc_pddis_en_va28(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_lcldo_enc_pddis_en_va28(
	void);
extern unsigned int mt6358_upmu_set_rg_lcldo_enc_remote_sense_va28(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_lcldo_enc_remote_sense_va28(
	void);
extern unsigned int mt6358_upmu_set_rg_va33refgen_en_va18(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_va33refgen_en_va18(
	void);
extern unsigned int mt6358_upmu_set_rg_va28refgen_en_va28(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_va28refgen_en_va28(
	void);
extern unsigned int mt6358_upmu_set_rg_hcldo_vosel_va18(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_hcldo_vosel_va18(
	void);
extern unsigned int mt6358_upmu_set_rg_lcldo_vosel_va18(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_lcldo_vosel_va18(
	void);
extern unsigned int mt6358_upmu_set_rg_nvreg_en_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_nvreg_en_vaudp15(
	void);
extern unsigned int mt6358_upmu_set_rg_nvreg_pull0v_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audpmu_rsd0_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audpmu_rsd0_va18(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audpmu_rsd0_va28(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audhpltrim_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audhprtrim_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audhplfinetrim_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audhprfinetrim_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audhptrim_en_vaudp15(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_audhptrim_en_vaudp15(
	void);
extern unsigned int mt6358_upmu_get_audzcd_ana_id(
	void);
extern unsigned int mt6358_upmu_get_audzcd_dig_id(
	void);
extern unsigned int mt6358_upmu_get_audzcd_ana_minor_rev(
	void);
extern unsigned int mt6358_upmu_get_audzcd_ana_major_rev(
	void);
extern unsigned int mt6358_upmu_get_audzcd_dig_minor_rev(
	void);
extern unsigned int mt6358_upmu_get_audzcd_dig_major_rev(
	void);
extern unsigned int mt6358_upmu_set_rg_audzcdenable(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audzcdgainsteptime(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audzcdgainstepsize(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audzcdtimeoutmodesel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audlolgain(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audlorgain(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audhplgain(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audhprgain(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audhsgain(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audivlgain(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audivrgain(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_audintgain1(
	void);
extern unsigned int mt6358_upmu_get_rg_audintgain2(
	void);
extern unsigned int mt6358_upmu_get_accdet_ana_id(
	void);
extern unsigned int mt6358_upmu_get_accdet_dig_id(
	void);
extern unsigned int mt6358_upmu_get_accdet_ana_minor_rev(
	void);
extern unsigned int mt6358_upmu_get_accdet_ana_major_rev(
	void);
extern unsigned int mt6358_upmu_get_accdet_dig_minor_rev(
	void);
extern unsigned int mt6358_upmu_get_accdet_dig_major_rev(
	void);
extern unsigned int mt6358_upmu_set_audaccdetauxadcswctrl(
	unsigned int val);
extern unsigned int mt6358_upmu_set_audaccdetauxadcswctrl_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_rg_audaccdetrsv(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_seq_init(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_eint0_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_eint0_seq_init(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_eint1_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_eint1_seq_init(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_anaswctrl_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_cmp_pwm_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_vth_pwm_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_mbias_pwm_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_eint0_pwm_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_eint1_pwm_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_cmp_pwm_idle(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_vth_pwm_idle(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_mbias_pwm_idle(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_eint0_pwm_idle(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_eint1_pwm_idle(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_pwm_width(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_pwm_thresh(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_rise_delay(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_fall_delay(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_debounce0(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_debounce1(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_debounce2(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_debounce3(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_debounce4(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_ival_cur_in(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_eint0_ival_cur_in(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_eint1_ival_cur_in(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_ival_sam_in(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_eint0_ival_sam_in(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_eint1_ival_sam_in(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_ival_mem_in(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_eint0_ival_mem_in(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_eint1_ival_mem_in(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_ival_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_eint0_ival_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_eint1_ival_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_get_accdet_irq(
	void);
extern unsigned int mt6358_upmu_get_accdet_eint0_irq(
	void);
extern unsigned int mt6358_upmu_get_accdet_eint1_irq(
	void);
extern unsigned int mt6358_upmu_set_accdet_irq_clr(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_eint0_irq_clr(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_eint1_irq_clr(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_eint0_irq_polarity(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_eint1_irq_polarity(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_test_mode0(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_cmp_swsel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_vth_swsel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_mbias_swsel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_test_mode4(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_test_mode5(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_pwm_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_in_sw(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_cmp_en_sw(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_vth_en_sw(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_mbias_en_sw(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_pwm_en_sw(
	unsigned int val);
extern unsigned int mt6358_upmu_get_accdet_in(
	void);
extern unsigned int mt6358_upmu_get_accdet_cur_in(
	void);
extern unsigned int mt6358_upmu_get_accdet_sam_in(
	void);
extern unsigned int mt6358_upmu_get_accdet_mem_in(
	void);
extern unsigned int mt6358_upmu_get_accdet_state(
	void);
extern unsigned int mt6358_upmu_get_accdet_mbias_clk(
	void);
extern unsigned int mt6358_upmu_get_accdet_vth_clk(
	void);
extern unsigned int mt6358_upmu_get_accdet_cmp_clk(
	void);
extern unsigned int mt6358_upmu_get_da_audaccdetauxadcswctrl(
	void);
extern unsigned int mt6358_upmu_set_accdet_eint0_deb_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_eint0_debounce(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_eint0_pwm_thresh(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_eint0_pwm_width(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_eint0_pwm_fall_delay(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_eint0_pwm_rise_delay(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_test_mode11(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_test_mode10(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_eint0_cmpout_sw(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_eint1_cmpout_sw(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_test_mode9(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_test_mode8(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_auxadc_ctrl_sw(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_test_mode7(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_test_mode6(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_eint0_cmp_en_sw(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_eint1_cmp_en_sw(
	unsigned int val);
extern unsigned int mt6358_upmu_get_accdet_eint0_state(
	void);
extern unsigned int mt6358_upmu_get_accdet_auxadc_debounce_end(
	void);
extern unsigned int mt6358_upmu_get_accdet_auxadc_connect_pre(
	void);
extern unsigned int mt6358_upmu_get_accdet_eint0_cur_in(
	void);
extern unsigned int mt6358_upmu_get_accdet_eint0_sam_in(
	void);
extern unsigned int mt6358_upmu_get_accdet_eint0_mem_in(
	void);
extern unsigned int mt6358_upmu_get_ad_eint0cmpout(
	void);
extern unsigned int mt6358_upmu_get_da_ni_eint0cmpen(
	void);
extern unsigned int mt6358_upmu_get_accdet_cur_deb(
	void);
extern unsigned int mt6358_upmu_get_accdet_eint0_cur_deb(
	void);
extern unsigned int mt6358_upmu_set_accdet_mon_flag_en(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_mon_flag_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_get_accdet_rsv_con1(
	void);
extern unsigned int mt6358_upmu_set_accdet_auxadc_connect_time(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_hwen_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_hwmode_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_eint_deb_out_dff(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_fast_discharge(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_eint0_reverse(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_eint1_reverse(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_eint1_deb_sel(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_eint1_debounce(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_eint1_pwm_thresh(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_eint1_pwm_width(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_eint1_pwm_fall_delay(
	unsigned int val);
extern unsigned int mt6358_upmu_set_accdet_eint1_pwm_rise_delay(
	unsigned int val);
extern unsigned int mt6358_upmu_get_accdet_eint1_state(
	void);
extern unsigned int mt6358_upmu_get_accdet_eint1_cur_in(
	void);
extern unsigned int mt6358_upmu_get_accdet_eint1_sam_in(
	void);
extern unsigned int mt6358_upmu_get_accdet_eint1_mem_in(
	void);
extern unsigned int mt6358_upmu_get_ad_eint1cmpout(
	void);
extern unsigned int mt6358_upmu_get_da_ni_eint1cmpen(
	void);
extern unsigned int mt6358_upmu_get_accdet_eint1_cur_deb(
	void);
extern unsigned int mt6358_upmu_set_rg_vcn33_bt_vosel(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vcn33_bt_vosel(
	void);
extern unsigned int mt6358_upmu_set_rg_vcn33_wifi_vosel(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_vcn33_wifi_vosel(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcn33_bt_dummy_load(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcn33_bt_dummy_load(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcn33_wifi_dummy_load(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcn33_wifi_dummy_load(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcn33_bt_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcn33_bt_hw0_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcn33_wifi_hw0_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcn33_wifi_hw0_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcn33_bt_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcn33_bt_hw1_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcn33_wifi_hw1_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcn33_wifi_hw1_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcn33_bt_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcn33_bt_hw2_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcn33_wifi_hw2_op_cfg(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcn33_wifi_hw2_op_cfg(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcn33_bt_hw0_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcn33_bt_hw0_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcn33_wifi_hw0_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcn33_wifi_hw0_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcn33_bt_hw1_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcn33_bt_hw1_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcn33_wifi_hw1_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcn33_wifi_hw1_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcn33_bt_hw2_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcn33_bt_hw2_op_en(
	void);
extern unsigned int mt6358_upmu_set_rg_ldo_vcn33_wifi_hw2_op_en(
	unsigned int val);
extern unsigned int mt6358_upmu_get_rg_ldo_vcn33_wifi_hw2_op_en(
	void);
extern unsigned int mt6358_upmu_get_da_vcn33_bt_en(
	void);
extern unsigned int mt6358_upmu_get_da_vcn33_wifi_en(
	void);
extern unsigned int mt6358_upmu_get_da_vcn33_bt_dummy_load(
	void);
extern unsigned int mt6358_upmu_get_da_vcn33_wifi_dummy_load(
	void);
extern unsigned int mt6358_upmu_get_da_vcn33_bt_stb(
	void);
extern unsigned int mt6358_upmu_get_da_vcn33_wifi_stb(
	void);
extern unsigned int mt6358_upmu_get_da_vcn33_bt_mode(
	void);
extern unsigned int mt6358_upmu_get_da_vcn33_wifi_mode(
	void);

#endif				/* _MT_PMIC_API_H_ */
