# SPDX-License-Identifier: GPL-2.0
ccflags-y := -I$(srctree)/include/drm -I$(srctree)/techpack/display/msm -I$(srctree)/techpack/display/msm/dsi -I$(srctree)/techpack/display/msm/dp
ccflags-y += -I$(srctree)/techpack/display/msm/sde
ccflags-y += -I$(srctree)/techpack/display/rotator
ccflags-y += -I$(srctree)/techpack/display/hdcp
ccflags-y += -I$(srctree)/drivers/clk/qcom/

msm_drm-$(CONFIG_DRM_MSM_DP) += dp/dp_altmode.o \
	dp/dp_parser.o \
	dp/dp_power.o \
	dp/dp_catalog.o \
	dp/dp_catalog_v420.o \
	dp/dp_catalog_v200.o \
	dp/dp_aux.o \
	dp/dp_panel.o \
	dp/dp_link.o \
	dp/dp_ctrl.o \
	dp/dp_audio.o \
	dp/dp_debug.o \
	dp/dp_hpd.o \
	dp/dp_gpio_hpd.o \
	dp/dp_lphw_hpd.o \
	dp/dp_display.o \
	dp/dp_drm.o \
	dp/dp_hdcp2p2.o \
	sde_hdcp_1x.o \
	sde_hdcp_2x.o \
	dp/dp_pll.o \
	dp/dp_pll_5nm.o

msm_drm-$(CONFIG_DRM_MSM_DP_MST) += dp/dp_mst_drm.o

msm_drm-$(CONFIG_DRM_MSM_DP_USBPD_LEGACY) += dp/dp_usbpd.o

msm_drm-$(CONFIG_DRM_MSM_SDE) += sde/sde_crtc.o \
	sde/sde_encoder.o \
	sde/sde_encoder_dce.o \
	sde/sde_encoder_phys_vid.o \
	sde/sde_encoder_phys_cmd.o \
	sde/sde_irq.o \
	sde/sde_core_irq.o \
	sde/sde_core_perf.o \
	sde/sde_rm.o \
	sde/sde_kms_utils.o \
	sde/sde_kms.o \
	sde/sde_plane.o \
	sde/sde_connector.o \
	sde/sde_color_processing.o \
	sde/sde_vbif.o \
	sde_io_util.o \
	sde_vm_event.o \
	sde/sde_hw_reg_dma_v1_color_proc.o \
	sde/sde_hw_color_proc_v4.o \
	sde/sde_hw_ad4.o \
	sde/sde_hw_uidle.o \
	sde_edid_parser.o \
	sde/sde_hw_catalog.o \
	sde/sde_hw_cdm.o \
	sde/sde_hw_dspp.o \
	sde/sde_hw_intf.o \
	sde/sde_hw_lm.o \
	sde/sde_hw_ctl.o \
	sde/sde_hw_util.o \
	sde/sde_hw_sspp.o \
	sde/sde_hw_wb.o \
	sde/sde_hw_pingpong.o \
	sde/sde_hw_top.o \
	sde/sde_hw_interrupts.o \
	sde/sde_hw_vbif.o \
	sde/sde_hw_blk.o \
	sde/sde_formats.o \
	sde_power_handle.o \
	sde/sde_hw_color_processing_v1_7.o \
	sde/sde_reg_dma.o \
	sde/sde_hw_reg_dma_v1.o \
	sde/sde_hw_dsc.o \
	sde/sde_hw_dsc_1_2.o \
	sde/sde_hw_vdc.o \
	sde/sde_hw_ds.o \
	sde/sde_fence.o \
	sde/sde_hw_qdss.o \
	sde_dsc_helper.o \
	sde_vdc_helper.o \
	sde/sde_hw_rc.o \
	sde_dbg.o \
	sde_dbg_evtlog.o \

msm_drm-$(CONFIG_DRM_SDE_VM) += sde/sde_vm_common.o \
	sde/sde_vm_primary.o \
	sde/sde_vm_trusted.o \
	sde/sde_vm_msgq.o

msm_drm-$(CONFIG_DRM_SDE_WB) += sde/sde_wb.o \
	sde/sde_encoder_phys_wb.o

msm_drm-$(CONFIG_DRM_SDE_RSC) += sde_rsc.o \
	sde_rsc_hw.o \
	sde_rsc_hw_v3.o

msm_drm-$(CONFIG_DRM_MSM_DSI) += dsi/dsi_phy.o \
	dsi/dsi_pwr.o \
	dsi/dsi_phy.o \
	dsi/dsi_phy_hw_v2_0.o \
	dsi/dsi_phy_hw_v3_0.o \
	dsi/dsi_phy_hw_v4_0.o \
	dsi/dsi_phy_timing_calc.o \
	dsi/dsi_phy_timing_v2_0.o \
	dsi/dsi_phy_timing_v3_0.o \
	dsi/dsi_phy_timing_v4_0.o \
	dsi/dsi_pll.o \
	dsi/dsi_pll_5nm.o \
	dsi/dsi_pll_10nm.o \
	dsi/dsi_ctrl_hw_cmn.o \
	dsi/dsi_ctrl_hw_1_4.o \
	dsi/dsi_ctrl_hw_2_0.o \
	dsi/dsi_ctrl_hw_2_2.o \
	dsi/dsi_ctrl.o \
	dsi/dsi_catalog.o \
	dsi/dsi_drm.o \
	dsi/dsi_display.o \
	dsi/dsi_panel.o \
	dsi/dsi_clk_manager.o \
	dsi/dsi_display_test.o

msm_drm-$(CONFIG_DSI_PARSER) += dsi/dsi_parser.o

msm_drm-$(CONFIG_DRM_MSM) += \
	msm_atomic.o \
	msm_fb.o \
	msm_iommu.o \
	msm_drv.o \
	msm_gem.o \
	msm_gem_prime.o \
	msm_gem_vma.o \
	msm_smmu.o \
	msm_cooling_device.o \
	msm_prop.o

msm_drm-$(CONFIG_HDCP_QSEECOM) += ../hdcp/msm_hdcp.o \

msm_drm-$(CONFIG_MSM_SDE_ROTATOR) += ../rotator/sde_rotator_dev.o \
	../rotator/sde_rotator_core.o \
	../rotator/sde_rotator_base.o \
	../rotator/sde_rotator_formats.o \
	../rotator/sde_rotator_util.o \
	../rotator/sde_rotator_io_util.o \
	../rotator/sde_rotator_smmu.o \
	../rotator/sde_rotator_r1_wb.o \
	../rotator/sde_rotator_r1_pipe.o \
	../rotator/sde_rotator_r1_ctl.o \
	../rotator/sde_rotator_r1.o \
	../rotator/sde_rotator_r3.o \
	../rotator/sde_rotator_sync.o \
	../rotator/sde_rotator_debug.o \
	../rotator/sde_rotator_r1_debug.o \
	../rotator/sde_rotator_r3_debug.o

obj-$(CONFIG_DISPLAY_BUILD)	+= msm_drm.o

obj-$(CONFIG_DRM_FBDEV_EMULATION) += msm_fbdev.o
