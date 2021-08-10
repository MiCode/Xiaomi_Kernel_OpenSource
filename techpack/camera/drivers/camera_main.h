/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#ifndef CAMERA_MAIN_H
#define CAMERA_MAIN_H

#include <linux/platform_device.h>
#include <linux/component.h>

extern struct platform_driver cam_sync_driver;
extern struct platform_driver cam_smmu_driver;
extern struct platform_driver cam_cpas_driver;
extern struct platform_driver cam_cdm_intf_driver;
extern struct platform_driver cam_hw_cdm_driver;
#ifdef CONFIG_SPECTRA_ISP
extern struct platform_driver cam_top_tpg_driver;
extern struct platform_driver cam_ife_csid17x_driver;
extern struct platform_driver cam_ife_csid_lite_driver;
extern struct platform_driver cam_vfe_driver;
#ifdef CONFIG_SPECTRA_SFE
extern struct platform_driver cam_sfe_driver;
#endif
extern struct platform_driver isp_driver;
#endif
#ifdef CONFIG_SPECTRA_TFE
extern struct platform_driver cam_csid_ppi100_driver;
extern struct platform_driver cam_tfe_driver;
extern struct platform_driver cam_tfe_csid530_driver;
#endif
#ifdef CONFIG_SPECTRA_SENSOR
extern struct platform_driver cam_res_mgr_driver;
extern struct platform_driver cci_driver;
extern struct platform_driver csiphy_driver;
extern struct platform_driver cam_actuator_platform_driver;
extern struct platform_driver cam_sensor_platform_driver;
extern struct platform_driver cam_eeprom_platform_driver;
extern struct platform_driver cam_ois_platform_driver;
#if IS_REACHABLE(CONFIG_LEDS_QPNP_FLASH_V2) || \
	IS_REACHABLE(CONFIG_LEDS_QTI_FLASH)
extern struct platform_driver cam_flash_platform_driver;
#endif
#endif
#ifdef CONFIG_SPECTRA_ICP
extern struct platform_driver cam_a5_driver;
extern struct platform_driver cam_ipe_driver;
extern struct platform_driver cam_bps_driver;
extern struct platform_driver cam_icp_driver;
#endif
#ifdef CONFIG_SPECTRA_OPE
extern struct platform_driver cam_ope_driver;
extern struct platform_driver cam_ope_subdev_driver;
#endif
#ifdef CONFIG_SPECTRA_JPEG
extern struct platform_driver cam_jpeg_enc_driver;
extern struct platform_driver cam_jpeg_dma_driver;
extern struct platform_driver jpeg_driver;
#endif
#ifdef CONFIG_SPECTRA_FD
extern struct platform_driver cam_fd_hw_driver;
extern struct platform_driver cam_fd_driver;
#endif
#ifdef CONFIG_SPECTRA_LRME
extern struct platform_driver cam_lrme_hw_driver;
extern struct platform_driver cam_lrme_driver;
#endif
#ifdef CONFIG_SPECTRA_CUSTOM
extern struct platform_driver cam_custom_hw_sub_mod_driver;
extern struct platform_driver cam_custom_csid_driver;
extern struct platform_driver custom_driver;
#endif

/*
 * Drivers to be bound by component framework in this order with
 * CRM as master
 */
static struct platform_driver *const cam_component_drivers[] = {
/* BASE */
	&cam_sync_driver,
	&cam_smmu_driver,
	&cam_cpas_driver,
	&cam_cdm_intf_driver,
	&cam_hw_cdm_driver,
#ifdef CONFIG_SPECTRA_TFE
	&cam_csid_ppi100_driver,
	&cam_tfe_driver,
	&cam_tfe_csid530_driver,
#endif
#ifdef CONFIG_SPECTRA_ISP
	&cam_top_tpg_driver,
	&cam_ife_csid17x_driver,
	&cam_ife_csid_lite_driver,
	&cam_vfe_driver,
#ifdef CONFIG_SPECTRA_SFE
	&cam_sfe_driver,
#endif
	&isp_driver,
#endif
#ifdef CONFIG_SPECTRA_SENSOR
	&cam_res_mgr_driver,
	&cci_driver,
	&csiphy_driver,
	&cam_actuator_platform_driver,
	&cam_sensor_platform_driver,
	&cam_eeprom_platform_driver,
	&cam_ois_platform_driver,
#if IS_REACHABLE(CONFIG_LEDS_QPNP_FLASH_V2) || \
	IS_REACHABLE(CONFIG_LEDS_QTI_FLASH)
	&cam_flash_platform_driver,
#endif
#endif
#ifdef CONFIG_SPECTRA_ICP
	&cam_a5_driver,
	&cam_ipe_driver,
	&cam_bps_driver,
	&cam_icp_driver,
#endif
#ifdef CONFIG_SPECTRA_OPE
	&cam_ope_driver,
	&cam_ope_subdev_driver,
#endif
#ifdef CONFIG_SPECTRA_JPEG
	&cam_jpeg_enc_driver,
	&cam_jpeg_dma_driver,
	&jpeg_driver,
#endif
#ifdef CONFIG_SPECTRA_FD
	&cam_fd_hw_driver,
	&cam_fd_driver,
#endif
#ifdef CONFIG_SPECTRA_LRME
	&cam_lrme_hw_driver,
	&cam_lrme_driver,
#endif
#ifdef CONFIG_SPECTRA_CUSTOM
	&cam_custom_hw_sub_mod_driver,
	&cam_custom_csid_driver,
	&custom_driver,
#endif
};

#endif /* CAMERA_MAIN_H */
