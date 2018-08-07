ccflags-y := -Iinclude/drm -Idrivers/gpu/drm/msm -Idrivers/gpu/drm/msm/dsi-staging
ccflags-y += -Idrivers/gpu/drm/msm/hdmi
ccflags-y += -Idrivers/gpu/drm/msm/hdmi-staging
ccflags-$(CONFIG_DRM_MSM_DSI) += -Idrivers/gpu/drm/msm/dsi
ccflags-$(CONFIG_SYNC) += -Idrivers/staging/android
ccflags-$(CONFIG_DRM_MSM_DSI_PLL) += -Idrivers/gpu/drm/msm/dsi
ccflags-y += -Idrivers/gpu/drm/msm/sde

msm_drm-y := \
	hdmi/hdmi.o \
	hdmi/hdmi_audio.o \
	hdmi/hdmi_bridge.o \
	hdmi/hdmi_connector.o \
	hdmi/hdmi_hdcp.o \
	hdmi/hdmi_i2c.o \
	hdmi/hdmi_util.o \
	hdmi/hdmi_phy_8960.o \
	hdmi/hdmi_phy_8x60.o \
	hdmi/hdmi_phy_8x74.o \
	edp/edp.o \
	edp/edp_aux.o \
	edp/edp_bridge.o \
	edp/edp_connector.o \
	edp/edp_ctrl.o \
	edp/edp_phy.o \
	mdp/mdp_format.o \
	mdp/mdp_kms.o \
	mdp/mdp5/mdp5_cfg.o \
	mdp/mdp5/mdp5_ctl.o \
	mdp/mdp5/mdp5_crtc.o \
	mdp/mdp5/mdp5_encoder.o \
	mdp/mdp5/mdp5_irq.o \
	mdp/mdp5/mdp5_kms.o \
	mdp/mdp5/mdp5_plane.o \
	mdp/mdp5/mdp5_smp.o \
	sde/sde_crtc.o \
	sde/sde_encoder.o \
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
	sde/sde_backlight.o \
	sde/sde_color_processing.o \
	sde/sde_vbif.o \
	sde/sde_splash.o \
	sde/sde_recovery_manager.o \
	sde_dbg.o \
	sde_dbg_evtlog.o \
	sde_io_util.o \
	dba_bridge.o \
	sde_edid_parser.o \
	sde_hdcp_1x.o

# use drm gpu driver only if qcom_kgsl driver not available
ifneq ($(CONFIG_QCOM_KGSL),y)
msm_drm-y += adreno/adreno_device.o \
	adreno/adreno_gpu.o \
	adreno/a3xx_gpu.o \
	adreno/a4xx_gpu.o \
	adreno/a5xx_gpu.o \
	adreno/a5xx_power.o \
	adreno/a5xx_preempt.o \
	adreno/a5xx_snapshot.o \
	adreno/a5xx_counters.o
endif

msm_drm-$(CONFIG_DRM_MSM_MDP4) += mdp/mdp4/mdp4_crtc.o \
	mdp/mdp4/mdp4_dtv_encoder.o \
	mdp/mdp4/mdp4_lcdc_encoder.o \
	mdp/mdp4/mdp4_lvds_connector.o \
	mdp/mdp4/mdp4_irq.o \
	mdp/mdp4/mdp4_kms.o \
	mdp/mdp4/mdp4_plane.o

msm_drm-$(CONFIG_DRM_FBDEV_EMULATION) += msm_fbdev.o
msm_drm-$(CONFIG_SYNC) += sde/sde_fence.o
msm_drm-$(CONFIG_COMMON_CLK) += mdp/mdp4/mdp4_lvds_pll.o

msm_drm-$(CONFIG_DRM_MSM_DSI) += dsi/dsi.o \
			dsi/dsi_cfg.o \
			dsi/dsi_host.o \
			dsi/dsi_manager.o \
			dsi/phy/dsi_phy.o \
			dsi/dsi_manager.o \
			mdp/mdp5/mdp5_cmd_encoder.o

msm_drm-$(CONFIG_DRM_MSM_DSI_28NM_PHY) += dsi/phy/dsi_phy_28nm.o
msm_drm-$(CONFIG_DRM_MSM_DSI_20NM_PHY) += dsi/phy/dsi_phy_20nm.o

msm_drm-$(CONFIG_DRM_MSM_DSI_STAGING) += dsi-staging/dsi_phy.o \
				dsi-staging/dsi_clk_pwr.o \
				dsi-staging/dsi_phy.o \
				dsi-staging/dsi_phy_hw_v4_0.o \
				dsi-staging/dsi_ctrl_hw_1_4.o \
				dsi-staging/dsi_ctrl.o \
				dsi-staging/dsi_catalog.o \
				dsi-staging/dsi_drm.o \
				dsi-staging/dsi_display.o \
				dsi-staging/dsi_panel.o \
				dsi-staging/dsi_display_test.o

msm_drm-$(CONFIG_DRM_SDE_HDMI) += \
	hdmi-staging/sde_hdmi_util.o \
	hdmi-staging/sde_hdmi.o \
	hdmi-staging/sde_hdmi_bridge.o \
	hdmi-staging/sde_hdmi_audio.o \
	hdmi-staging/sde_hdmi_hdcp2p2.o \

msm_drm-$(CONFIG_DRM_MSM_DSI_PLL) += dsi/pll/dsi_pll.o \
				dsi/pll/dsi_pll_28nm.o

msm_drm-$(CONFIG_DRM_MSM) += \
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
	sde/sde_formats.o \
	sde_power_handle.o \
	sde/sde_hw_color_processing_v1_7.o

msm_drm-$(CONFIG_DRM_SDE_WB) += sde/sde_wb.o \
	sde/sde_encoder_phys_wb.o

msm_drm-$(CONFIG_DRM_MSM) += \
	msm_atomic.o \
	msm_drv.o \
	msm_fb.o \
	msm_gem.o \
	msm_gem_prime.o \
	msm_gem_submit.o \
	msm_gem_vma.o \
	msm_gpu.o \
	msm_iommu.o \
	msm_smmu.o \
	msm_perf.o \
	msm_rd.o \
	msm_ringbuffer.o \
	msm_prop.o \
	msm_snapshot.o \
	msm_submitqueue.o \
	msm_trace_points.o

obj-$(CONFIG_DRM_MSM)	+= msm_drm.o
