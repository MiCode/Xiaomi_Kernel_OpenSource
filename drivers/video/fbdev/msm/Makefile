# SPDX-License-Identifier: GPL-2.0-only
ccflags-y += -I$(src)

obj-$(CONFIG_FB_MSM_MDSS_MHL3) += mhl3/
obj-$(CONFIG_MSM_DBA) += msm_dba/

ifeq ($(CONFIG_FB_MSM_MDSS_MDP3), y)
ccflags-y += -DTARGET_HW_MDSS_MDP3
endif
mdss-mdp-objs := mdss_mdp.o mdss_mdp_ctl.o mdss_mdp_pipe.o mdss_mdp_util.o dsi_status_6g.o
mdss-mdp-objs += mdss_mdp_pp.o mdss_mdp_pp_debug.o mdss_mdp_pp_cache_config.o mdss_sync.o
mdss-mdp-objs += mdss_mdp_intf_video.o
mdss-mdp-objs += mdss_mdp_intf_cmd.o
mdss-mdp-objs += mdss_mdp_intf_writeback.o
mdss-mdp-objs += mdss_mdp_overlay.o
mdss-mdp-objs += mdss_mdp_layer.o
mdss-mdp-objs += mdss_mdp_splash_logo.o
mdss-mdp-objs += mdss_mdp_cdm.o
mdss-mdp-objs += mdss_smmu.o
mdss-mdp-objs += mdss_mdp_wfd.o
mdss-mdp-objs += mdss_io_util.o
obj-$(CONFIG_FB_MSM_MDSS) += mdss-mdp.o
obj-$(CONFIG_FB_MSM_MDSS) += mdss_mdp_debug.o

mdss-mdp-objs += mdss_mdp_pp_v1_7.o
mdss-mdp-objs += mdss_mdp_pp_v3.o
mdss-mdp-objs += mdss_mdp_pp_common.o
mdss-mdp-objs += mdss_mdp_pp_stub.o

ifeq ($(CONFIG_FB_MSM_MDSS),y)
obj-$(CONFIG_DEBUG_FS) += mdss_debug.o mdss_debug_xlog.o
endif

ifeq ($(CONFIG_FB_MSM_MDSS_FRC_DEBUG),y)
obj-$(CONFIG_DEBUG_FS) += mdss_debug_frc.o
endif

mdss-dsi-objs := mdss_dsi.o mdss_dsi_host.o mdss_dsi_cmd.o mdss_dsi_status.o
mdss-dsi-objs += mdss_dsi_panel.o
mdss-dsi-objs += msm_mdss_io_8974.o
mdss-dsi-objs += mdss_dsi_phy.o
mdss-dsi-objs += mdss_dsi_phy_v3.o
mdss-dsi-objs += mdss_dsi_clk.o
obj-$(CONFIG_FB_MSM_MDSS) += mdss-dsi.o
obj-$(CONFIG_FB_MSM_MDSS) += mdss_panel.o

ifneq ($(CONFIG_FB_MSM_MDSS_MDP3), y)
obj-$(CONFIG_FB_MSM_MDSS) += mdss_hdmi_util.o
obj-$(CONFIG_FB_MSM_MDSS) += mdss_hdmi_edid.o
obj-$(CONFIG_FB_MSM_MDSS) += mdss_cec_core.o
obj-$(CONFIG_FB_MSM_MDSS) += mdss_dba_utils.o

obj-$(CONFIG_FB_MSM_MDSS_HDMI_PANEL) += mdss_hdmi_tx.o
obj-$(CONFIG_FB_MSM_MDSS_HDMI_PANEL) += mdss_hdmi_panel.o
obj-$(CONFIG_FB_MSM_MDSS_HDMI_PANEL) += mdss_hdcp_1x.o
obj-$(CONFIG_FB_MSM_MDSS_HDMI_PANEL) += mdss_hdcp_2x.o
obj-$(CONFIG_FB_MSM_MDSS_HDMI_PANEL) += mdss_hdmi_hdcp2p2.o
obj-$(CONFIG_FB_MSM_MDSS_HDMI_PANEL) += mdss_hdmi_cec.o
obj-$(CONFIG_FB_MSM_MDSS_HDMI_PANEL) += mdss_hdmi_audio.o
obj-$(CONFIG_FB_MSM_MDSS_HDMI_MHL_SII8334) += mhl_sii8334.o mhl_msc.o
ccflags-y += -DTARGET_HW_MDSS_HDMI
endif

obj-$(CONFIG_FB_MSM_MDSS_DP_PANEL) += mdss_dp.o mdss_dp_util.o
obj-$(CONFIG_FB_MSM_MDSS_DP_PANEL) += mdss_dp_aux.o
obj-$(CONFIG_FB_MSM_MDSS_DP_PANEL) += mdss_dp_hdcp2p2.o

obj-$(CONFIG_FB_MSM_MDSS_WRITEBACK) += mdss_wb.o

obj-$(CONFIG_FB_MSM_MDSS) += mdss_fb.o mdss_util.o
obj-$(CONFIG_COMPAT) += mdss_compat_utils.o
