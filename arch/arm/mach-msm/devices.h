/* linux/arch/arm/mach-msm/devices.h
 *
 * Copyright (C) 2008 Google, Inc.
 * Copyright (c) 2009-2013, The Linux Foundation. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __ARCH_ARM_MACH_MSM_DEVICES_H
#define __ARCH_ARM_MACH_MSM_DEVICES_H

#include <linux/clkdev.h>
#include <linux/platform_device.h>
#include "clock.h"

void __init msm9615_device_init(void);
void __init msm9615_map_io(void);
void __init msm_map_msm9615_io(void);
void __init msm9615_init_irq(void);
void __init msm_rotator_update_bus_vectors(unsigned int xres,
	unsigned int yres);
void __init msm_rotator_set_split_iommu_domain(void);

extern struct platform_device asoc_msm_pcm;
extern struct platform_device asoc_msm_dai0;
extern struct platform_device asoc_msm_dai1;
#if defined (CONFIG_SND_MSM_MVS_DAI_SOC)
extern struct platform_device asoc_msm_mvs;
extern struct platform_device asoc_mvs_dai0;
extern struct platform_device asoc_mvs_dai1;
#endif

extern struct platform_device msm_ebi0_thermal;
extern struct platform_device msm_ebi1_thermal;

extern struct platform_device msm_adsp_device;
extern struct platform_device msm_device_uart1;
extern struct platform_device msm_device_uart2;
extern struct platform_device msm_device_uart3;
extern struct platform_device msm8625_device_uart1;

extern struct platform_device msm_device_uart_dm1;
extern struct platform_device msm_device_uart_dm2;
extern struct platform_device msm_device_uart_dm3;
extern struct platform_device msm_device_uart_dm12;
extern struct platform_device *msm_device_uart_gsbi9;
extern struct platform_device msm_device_uart_dm6;
extern struct platform_device msm_device_uart_dm8;
extern struct platform_device msm_device_uart_dm9;
extern struct platform_device mpq8064_device_uartdm_gsbi6;
extern struct platform_device mpq8064_device_uart_gsbi5;

extern struct platform_device msm8960_device_uart_gsbi2;
extern struct platform_device msm8960_device_uart_gsbi5;
extern struct platform_device msm8960_device_uart_gsbi8;
extern struct platform_device msm8960_device_ssbi_pmic;
extern struct platform_device msm8960_device_qup_i2c_gsbi3;
extern struct platform_device msm8960_device_qup_i2c_gsbi4;
extern struct platform_device msm8960_device_qup_i2c_gsbi9;
extern struct platform_device msm8960_device_qup_i2c_gsbi10;
extern struct platform_device msm8960_device_qup_i2c_gsbi12;
extern struct platform_device msm8960_device_qup_spi_gsbi1;
extern struct platform_device msm8960_gemini_device;
extern struct platform_device msm8960_mercury_device;
extern struct platform_device msm8960_device_i2c_mux_gsbi4;
extern struct platform_device msm8960_device_csiphy0;
extern struct platform_device msm8960_device_csiphy1;
extern struct platform_device msm8960_device_csiphy2;
extern struct platform_device msm8960_device_csid0;
extern struct platform_device msm8960_device_csid1;
extern struct platform_device msm8960_device_csid2;
extern struct platform_device msm8960_device_ispif;
extern struct platform_device msm8960_device_vfe;
extern struct platform_device msm8960_device_vpe;
extern struct platform_device msm8960_device_cache_erp;
extern struct platform_device msm8960_device_ebi1_ch0_erp;
extern struct platform_device msm8960_device_ebi1_ch1_erp;

extern struct platform_device apq8064_device_uart_gsbi1;
extern struct platform_device apq8064_device_uart_gsbi2;
extern struct platform_device apq8064_device_uart_gsbi3;
extern struct platform_device apq8064_device_uart_gsbi4;
extern struct platform_device apq8064_device_uart_gsbi7;
extern struct platform_device apq8064_device_qup_i2c_gsbi1;
extern struct platform_device apq8064_device_qup_i2c_gsbi3;
extern struct platform_device apq8064_device_qup_i2c_gsbi4;
extern struct platform_device apq8064_device_qup_spi_gsbi5;
extern struct platform_device apq8064_slim_ctrl;
extern struct platform_device apq8064_device_ssbi_pmic1;
extern struct platform_device apq8064_device_ssbi_pmic2;
extern struct platform_device apq8064_device_cache_erp;
extern struct platform_device apq8064_device_sata;

extern struct platform_device msm9615_device_uart_gsbi4;
extern struct platform_device msm9615_device_qup_i2c_gsbi5;
extern struct platform_device msm9615_device_qup_spi_gsbi3;
extern struct platform_device msm9615_slim_ctrl;
extern struct platform_device msm9615_device_ssbi_pmic1;
extern struct platform_device msm9615_device_tsens;
extern struct platform_device msm_bus_9615_sys_fabric;
extern struct platform_device msm_bus_def_fab;

extern struct platform_device msm_device_sdc1;
extern struct platform_device msm_device_sdc2;
extern struct platform_device msm_device_sdc3;
extern struct platform_device msm_device_sdc4;

extern struct platform_device msm8960_pm_8x60;
extern struct platform_device msm8064_pm_8x60;
extern struct platform_device msm8930_pm_8x60;
extern struct platform_device msm9615_pm_8x60;
extern struct platform_device msm8660_pm_8x60;

extern struct platform_device msm_device_gadget_peripheral;
extern struct platform_device msm_device_hsusb_host;
extern struct platform_device msm_device_hsusb_host2;
extern struct platform_device msm_device_hsic_host;

extern struct platform_device msm8960_cpu_slp_status;
extern struct platform_device msm8064_cpu_slp_status;
extern struct platform_device msm8930_cpu_slp_status;

extern struct platform_device msm_device_otg;
extern struct platform_device msm_android_usb_device;
extern struct platform_device msm_android_usb_hsic_device;
extern struct platform_device msm_device_hsic_peripheral;
extern struct platform_device msm8960_device_otg;
extern struct platform_device msm8960_device_gadget_peripheral;

extern struct platform_device apq8064_device_otg;
extern struct platform_device apq8064_usb_diag_device;
extern struct platform_device apq8064_device_gadget_peripheral;
extern struct platform_device apq8064_device_hsusb_host;
extern struct platform_device apq8064_device_hsic_host;
extern struct platform_device apq8064_device_ehci_host3;
extern struct platform_device apq8064_device_ehci_host4;

extern struct platform_device msm_device_i2c;

extern struct platform_device msm_device_i2c_2;

extern struct platform_device qup_device_i2c;

extern struct platform_device msm_gsbi0_qup_i2c_device;
extern struct platform_device msm_gsbi1_qup_i2c_device;
extern struct platform_device msm_gsbi3_qup_i2c_device;
extern struct platform_device msm_gsbi4_qup_i2c_device;
extern struct platform_device msm_gsbi7_qup_i2c_device;
extern struct platform_device msm_gsbi8_qup_i2c_device;
extern struct platform_device msm_gsbi9_qup_i2c_device;
extern struct platform_device msm_gsbi12_qup_i2c_device;

extern struct platform_device msm8625_gsbi0_qup_i2c_device;
extern struct platform_device msm8625_gsbi1_qup_i2c_device;
extern struct platform_device msm8625_device_uart_dm1;
extern struct platform_device msm8625_device_uart_dm2;
extern struct platform_device msm8625_device_sdc1;
extern struct platform_device msm8625_device_sdc2;
extern struct platform_device msm8625_device_sdc3;
extern struct platform_device msm8625_device_sdc4;
extern struct platform_device msm8625_device_gadget_peripheral;
extern struct platform_device msm8625_device_hsusb_host;
extern struct platform_device msm8625_device_otg;
extern struct platform_device msm8625_kgsl_3d0;
extern struct platform_device msm8625_device_adsp;

extern struct platform_device msm_slim_ctrl;
extern struct platform_device msm_device_sps;
extern struct platform_device msm_device_usb_bam;
extern struct platform_device msm_device_sps_apq8064;
extern struct platform_device msm_device_bam_dmux;
extern struct platform_device msm_device_smd;
extern struct platform_device msm_device_smd_apq8064;
extern struct platform_device msm8625_device_smd;
extern struct platform_device msm_device_dmov;
extern struct platform_device msm8960_device_dmov;
extern struct platform_device apq8064_device_dmov;
extern struct platform_device msm9615_device_dmov;
extern struct platform_device msm8625_device_dmov;
extern struct platform_device msm_device_dmov_adm0;
extern struct platform_device msm_device_dmov_adm1;

extern struct platform_device msm_device_pcie;

extern struct platform_device msm_device_nand;

extern struct platform_device msm_device_tssc;

extern struct platform_device msm_rotator_device;
#ifdef CONFIG_MSM_VCAP
extern struct platform_device msm8064_device_vcap;
#endif

#ifdef CONFIG_MSM_BUS_SCALING
extern struct msm_bus_scale_pdata rotator_bus_scale_pdata;
#endif

extern struct platform_device msm_device_tsif[2];
extern struct platform_device msm_8064_device_tsif[2];
extern struct platform_device msm_8064_device_tspp;

extern struct platform_device msm_device_ssbi_pmic1;
extern struct platform_device msm_device_ssbi_pmic2;
extern struct platform_device msm_device_ssbi1;
extern struct platform_device msm_device_ssbi2;
extern struct platform_device msm_device_ssbi3;
extern struct platform_device msm_device_ssbi6;
extern struct platform_device msm_device_ssbi7;

extern struct platform_device msm_gsbi1_qup_spi_device;

extern struct platform_device msm_device_vidc_720p;

extern struct platform_device msm_pcm;
extern struct platform_device msm_multi_ch_pcm;
extern struct platform_device msm_lowlatency_pcm;
extern struct platform_device msm_pcm_routing;
extern struct platform_device msm_cpudai0;
extern struct platform_device msm_cpudai1;
extern struct platform_device mpq_cpudai_sec_i2s_rx;
extern struct platform_device mpq_cpudai_pseudo;
extern struct platform_device msm8960_cpudai_slimbus_2_rx;
extern struct platform_device msm8960_cpudai_slimbus_2_tx;
extern struct platform_device msm_cpudai_hdmi_rx;
extern struct platform_device msm_cpudai_bt_rx;
extern struct platform_device msm_cpudai_bt_tx;
extern struct platform_device msm_cpudai_fm_rx;
extern struct platform_device msm_cpudai_fm_tx;
extern struct platform_device msm_cpudai_auxpcm_rx;
extern struct platform_device msm_cpudai_auxpcm_tx;
extern struct platform_device msm_cpudai_sec_auxpcm_rx;
extern struct platform_device msm_cpudai_sec_auxpcm_tx;
extern struct platform_device msm_cpu_fe;
extern struct platform_device msm_stub_codec;
extern struct platform_device msm_voice;
extern struct platform_device msm_voip;
extern struct platform_device msm_dtmf;
extern struct platform_device msm_host_pcm_voice;
extern struct platform_device msm_lpa_pcm;
extern struct platform_device msm_pcm_hostless;
extern struct platform_device msm_cpudai_afe_01_rx;
extern struct platform_device msm_cpudai_afe_01_tx;
extern struct platform_device msm_cpudai_afe_02_rx;
extern struct platform_device msm_cpudai_afe_02_tx;
extern struct platform_device msm_pcm_afe;
extern struct platform_device msm_compr_dsp;
extern struct platform_device msm_cpudai_incall_music_rx;
extern struct platform_device msm_cpudai_incall_record_rx;
extern struct platform_device msm_cpudai_incall_record_tx;
extern struct platform_device msm_i2s_cpudai0;
extern struct platform_device msm_i2s_cpudai1;
extern struct platform_device msm_i2s_cpudai4;
extern struct platform_device msm_i2s_cpudai5;
extern struct platform_device msm_cpudai_stub;
extern struct platform_device msm_fm_loopback;

extern struct platform_device msm_pil_q6v3;
extern struct platform_device msm_pil_modem;
extern struct platform_device msm_pil_tzapps;
extern struct platform_device msm_pil_dsps;
extern struct platform_device msm_pil_vidc;
extern struct platform_device msm_8960_q6_lpass;
extern struct platform_device msm_9615_q6_lpass;
extern struct platform_device msm_8960_q6_mss;
extern struct platform_device msm_9615_q6_mss;
extern struct platform_device msm_8960_riva;
extern struct platform_device msm_gss;

extern struct platform_device apq_pcm;
extern struct platform_device apq_pcm_routing;
extern struct platform_device apq_cpudai0;
extern struct platform_device apq_cpudai1;
extern struct platform_device mpq_cpudai_mi2s_tx;
extern struct platform_device apq_cpudai_hdmi_rx;
extern struct platform_device apq_cpudai_bt_rx;
extern struct platform_device apq_cpudai_bt_tx;
extern struct platform_device apq_cpudai_fm_rx;
extern struct platform_device apq_cpudai_fm_tx;
extern struct platform_device apq_cpudai_auxpcm_rx;
extern struct platform_device apq_cpudai_auxpcm_tx;
extern struct platform_device apq_cpu_fe;
extern struct platform_device apq_stub_codec;
extern struct platform_device apq_voice;
extern struct platform_device apq_voip;
extern struct platform_device apq_lpa_pcm;
extern struct platform_device apq_compr_dsp;
extern struct platform_device apq_multi_ch_pcm;
extern struct platform_device apq_lowlatency_pcm;
extern struct platform_device apq_pcm_hostless;
extern struct platform_device apq_cpudai_afe_01_rx;
extern struct platform_device apq_cpudai_afe_01_tx;
extern struct platform_device apq_cpudai_afe_02_rx;
extern struct platform_device apq_cpudai_afe_02_tx;
extern struct platform_device apq_pcm_afe;
extern struct platform_device apq_cpudai_stub;
extern struct platform_device apq_cpudai_slimbus_1_rx;
extern struct platform_device apq_cpudai_slimbus_1_tx;
extern struct platform_device apq_cpudai_slimbus_2_rx;
extern struct platform_device apq_cpudai_slimbus_2_tx;
extern struct platform_device apq_cpudai_slimbus_3_rx;
extern struct platform_device apq_cpudai_slimbus_3_tx;
extern struct platform_device apq_cpudai_slim_4_rx;
extern struct platform_device apq_cpudai_slim_4_tx;

extern struct platform_device *msm_footswitch_devices[];
extern unsigned msm_num_footswitch_devices;
extern struct platform_device *msm8660_footswitch[];
extern unsigned msm8660_num_footswitch;
extern struct platform_device *msm8960_footswitch[];
extern unsigned msm8960_num_footswitch;
extern struct platform_device *msm8960ab_footswitch[];
extern unsigned msm8960ab_num_footswitch;
extern struct platform_device *apq8064_footswitch[];
extern unsigned apq8064_num_footswitch;
extern struct platform_device *msm8930_footswitch[];
extern unsigned msm8930_num_footswitch;
extern struct platform_device *msm8930_pm8917_footswitch[];
extern unsigned msm8930_pm8917_num_footswitch;
extern struct platform_device *msm8627_footswitch[];
extern unsigned msm8627_num_footswitch;

extern struct platform_device fsm_qfp_fuse_device;

extern struct platform_device fsm_xo_device;

extern struct platform_device qfec_device;

extern struct platform_device msm_kgsl_3d0;
extern struct platform_device msm_kgsl_2d0;
extern struct platform_device msm_kgsl_2d1;

extern struct resource kgsl_3d0_resources_8960ab[];
extern int kgsl_num_resources_8960ab;
#ifdef CONFIG_MSM_BUS_SCALING
extern struct msm_bus_scale_pdata grp3d_bus_scale_pdata_ab;
#endif

extern struct platform_device msm_mipi_dsi1_device;
extern struct platform_device mipi_dsi_device;
extern struct platform_device msm_lcdc_device;
extern struct platform_device msm_lvds_device;
extern struct platform_device msm_ebi2_lcdc_device;

extern struct clk_lookup msm_clocks_fsm9xxx[];
extern unsigned msm_num_clocks_fsm9xxx;

extern struct platform_device msm_footswitch;

void __init msm_fb_register_device(char *name, void *data);
void __init msm_camera_register_device(void *, uint32_t, void *);
struct platform_device *msm_add_gsbi9_uart(void);
extern struct platform_device msm_device_touchscreen;

extern struct platform_device led_pdev;

extern struct platform_device msm8960_rpm_device;
extern struct platform_device msm8960_rpm_stat_device;
extern struct platform_device msm8960_rpm_master_stat_device;
extern struct platform_device msm8960_rpm_log_device;

extern struct platform_device msm8930_rpm_device;
extern struct platform_device msm8930_rpm_stat_device;
extern struct platform_device msm8930_rpm_master_stat_device;
extern struct platform_device msm8930_rpm_log_device;
extern struct platform_device msm8930_rpm_rbcpr_device;

extern struct platform_device msm8660_rpm_device;
extern struct platform_device msm8660_rpm_stat_device;
extern struct platform_device msm8660_rpm_log_device;

extern struct platform_device msm9615_rpm_device;
extern struct platform_device msm9615_rpm_stat_device;
extern struct platform_device msm9615_rpm_master_stat_device;
extern struct platform_device msm9615_rpm_log_device;

extern struct platform_device apq8064_rpm_device;
extern struct platform_device apq8064_rpm_stat_device;
extern struct platform_device apq8064_rpm_master_stat_device;
extern struct platform_device apq8064_rpm_log_device;

extern struct platform_device msm_device_rng;
extern struct platform_device apq8064_device_rng;

#if defined(CONFIG_CRYPTO_DEV_QCRYPTO) || \
		defined(CONFIG_CRYPTO_DEV_QCRYPTO_MODULE)
extern struct platform_device msm9615_qcrypto_device;
#endif

#if defined(CONFIG_CRYPTO_DEV_QCEDEV) || \
		defined(CONFIG_CRYPTO_DEV_QCEDEV_MODULE)
extern struct platform_device msm9615_qcedev_device;
#endif
extern struct platform_device msm8960_device_watchdog;
extern struct platform_device msm8660_device_watchdog;
extern struct platform_device msm8064_device_watchdog;
extern struct platform_device msm9615_device_watchdog;
extern struct platform_device fsm9xxx_device_watchdog;

extern struct platform_device coresight_tpiu_device;
extern struct platform_device coresight_etb_device;
extern struct platform_device coresight_funnel_device;
extern struct platform_device apq8064_coresight_funnel_device;
extern struct platform_device coresight_etm0_device;
extern struct platform_device coresight_etm1_device;
extern struct platform_device coresight_etm2_device;
extern struct platform_device coresight_etm3_device;
#endif

extern struct platform_device msm_bus_8064_apps_fabric;
extern struct platform_device msm_bus_8064_sys_fabric;
extern struct platform_device msm_bus_8064_mm_fabric;
extern struct platform_device msm_bus_8064_sys_fpb;
extern struct platform_device msm_bus_8064_cpss_fpb;

extern struct platform_device mdm_8064_device;
extern struct platform_device i2s_mdm_8064_device;
extern struct platform_device msm_dsps_device_8064;
extern struct platform_device *msm_8974_stub_regulator_devices[];
extern int msm_8974_stub_regulator_devices_len;

extern struct platform_device apq8064_dcvs_device;
extern struct platform_device apq8064_msm_gov_device;

extern struct platform_device msm_bus_8930_apps_fabric;
extern struct platform_device msm_bus_8930_sys_fabric;
extern struct platform_device msm_bus_8930_mm_fabric;
extern struct platform_device msm_bus_8930_sys_fpb;
extern struct platform_device msm_bus_8930_cpss_fpb;

extern struct platform_device msm_device_csic0;
extern struct platform_device msm_device_csic1;
extern struct platform_device msm_device_vfe;
extern struct platform_device msm_device_vpe;
extern struct platform_device mpq8064_device_qup_i2c_gsbi5;
extern struct platform_device mpq8064_device_qup_spi_gsbi6;

extern struct platform_device msm8660_iommu_domain_device;
extern struct platform_device msm8960_iommu_domain_device;
extern struct platform_device msm8930_iommu_domain_device;
extern struct platform_device apq8064_iommu_domain_device;

extern struct platform_device msm8960_rtb_device;
extern struct platform_device msm8930_rtb_device;
extern struct platform_device apq8064_rtb_device;

extern struct platform_device msm8960_cache_dump_device;
extern struct platform_device apq8064_cache_dump_device;
extern struct platform_device msm8930_cache_dump_device;

extern struct platform_device mdm_sglte_device;

extern struct platform_device apq_device_tz_log;

extern struct platform_device msm7x27_device_acpuclk;
extern struct platform_device msm7x27a_device_acpuclk;
extern struct platform_device msm7x27aa_device_acpuclk;
extern struct platform_device msm7x30_device_acpuclk;
extern struct platform_device apq8064_device_acpuclk;
extern struct platform_device msm8625_device_acpuclk;
extern struct platform_device msm8627_device_acpuclk;
extern struct platform_device msm8625ab_device_acpuclk;
extern struct platform_device msm8x50_device_acpuclk;
extern struct platform_device msm8x60_device_acpuclk;
extern struct platform_device msm8930_device_acpuclk;
extern struct platform_device msm8930aa_device_acpuclk;
extern struct platform_device msm8930ab_device_acpuclk;
extern struct platform_device msm8960_device_acpuclk;
extern struct platform_device msm8960ab_device_acpuclk;
extern struct platform_device msm9615_device_acpuclk;

extern struct platform_device apq8064_msm_mpd_device;

extern struct platform_device msm_gpio_device;

extern struct platform_device apq_cpudai_mi2s;
extern struct platform_device apq_cpudai_i2s_rx;
extern struct platform_device apq_cpudai_i2s_tx;
extern struct dev_avtimer_data dev_avtimer_pdata;

